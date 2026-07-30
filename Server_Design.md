# Kung-Fu Chess — Scaled Server Architecture

**Scope.** This document proposes how to evolve Kung-Fu Chess's current backend — a single-process, single-threaded C++ server built on IXWebSocket, with an in-memory `game_id → GameSession` map and SQLite persistence — into an architecture that supports **100,000,000 registered users** and **10,000,000 concurrent players**. It is a system-design document (components, data flow, capacity reasoning), not an implementation plan; Kubernetes/K3s specifics are kept conceptual throughout, since that layer is being learned separately.

Every numeric claim below is derived from the assignment's stated numbers (10M concurrent players, one move roughly every 2 seconds, 30–90s matches) plus explicit, stated assumptions about message size and per-message processing cost. Where an assumption is not directly given by the assignment, it is called out so the calculation can be checked or challenged.

**Requirement traceability** — every explicit requirement, from both the written assignment and the follow-up mentor Zoom session, mapped to where it's addressed:

| Requirement | Addressed in |
|---|---|
| 100M registered users / DB choice | Section 1 |
| 10M concurrent users / server-to-player routing | Section 2 |
| "Everyone can play with everyone" / join any room | Section 2.4 |
| Network traffic volume calculation | Section 3 |
| Match duration (30–90s) → implications for Docker roles | Section 4 |
| Action plan for server or database failure (mentor Zoom) | Section 5 |
| CAP theorem framing (mentor Zoom) | Section 1.3 |
| Message queue as an async-processing pattern (mentor Zoom) | Section 1.3, point 4 |
| Observability (logs, metrics, alerts, load tests) | Section 2.6 |
| Service Discovery | Section 2.7 |
| End-to-end game lifecycle | Section 2.8 |
| Out of scope items | Section 6 |

---

## 1. Data Layer at Scale

### 1.1 Why not SQLite

SQLite's single-writer, single-host model can't sustain the write throughput this system requires at scale — see `Server_Design_Rationale_and_Open_Questions.md` for the full case.

Given 10M concurrent players in roughly 1-on-1 matches, that's up to ~5,000,000 concurrent matches; at an average match length of 60s (midpoint of the 30–90s range), the system completes matches — and so needs to apply rating-update writes — at a sustained peak rate of:

```
5,000,000 concurrent matches / 60 s per match ≈ 83,333 matches completing per second
83,333 matches/s × 2 writes ≈ 166,667 rating-update writes per second (peak, system-wide)
```

This peak write rate is the sizing basis for the Postgres write-sharding plan in Section 1.3, point 2.

### 1.2 Access pattern

Before picking a replacement, it's worth being precise about what we're actually optimizing for:

- **Reads dominate**: login/credential lookups (by username) and ELO lookups (for matchmaking's ±100 band scan) happen far more often than writes — every player who queues for a match triggers a read, but only players who *finish* a match trigger a write.
- **Writes are narrow and keyed**: rating updates touch exactly the two rows for the two players in a just-finished match, keyed by `user_id`. There is no wide-fan-out write (no single row being hammered by many concurrent writers), which matters for choosing a scaling strategy.
- **Rating updates need transactional correctness**: an ELO update is a read-modify-write (new rating depends on both players' prior ratings and the match result). Losing or double-applying an update corrupts the ranking system, which is core to a competitive game. This pushes toward a store with real transactions, not an eventually-consistent one, for this specific write path.

### 1.3 Recommendation: PostgreSQL, with read replicas and hash-sharding by `user_id`

**Why relational at all, instead of jumping straight to NoSQL:** the data itself is small and simple — a user row is essentially `(user_id, username, password_hash, rating, ...)` — and the queries are simple keyed lookups plus one small range query (rating ± 100 for matchmaking). This is a textbook relational workload; nothing about our access pattern needs the flexible-schema or wide-column features that justify a document/wide-column store. What we *do* need is transactional integrity on the ELO write path, which a strongly-consistent relational database gives us for free, and which many NoSQL stores explicitly trade away for availability under network partitions — this is precisely the consistency-vs-availability choice the **CAP theorem** describes: Postgres here is a CP choice (consistency over availability during a partition), where many NoSQL stores default to AP. Given rating integrity is a correctness property of the game, not a nice-to-have, this tips the recommendation toward Postgres over e.g. DynamoDB/Cassandra as the *primary* store.

**Concrete shape of the recommendation:**

1. **Read replicas** (Postgres streaming replication): the primary handles writes; N read replicas absorb the read-heavy login/rating-lookup traffic, fanned out behind the same connection layer the Auth and Matchmaking services use. This alone addresses most of the load, since reads vastly outnumber writes. One consequence worth flagging: streaming replication is asynchronous, so a replica can lag the primary by a small but non-zero interval — a player who just registered, or whose rating just changed, could hit a replica that hasn't caught up yet (a spurious "user not found" right after signup, or a stale rating during matchmaking). Fix (an implementation detail, not a full design): apply a **read-your-writes** strategy for the immediate post-write window — route the next read for a given `user_id` to the primary (or to a replica confirmed caught-up via replication-lag tracking) for a few seconds after that user's own write, falling back to any replica once the window passes.
2. **Write sharding by `user_id`** once a single primary's write throughput becomes the bottleneck. A single well-tuned Postgres primary on fast NVMe realistically sustains on the order of **10,000–30,000 simple indexed writes/sec** — our computed peak of ~166,667 writes/sec exceeds that by roughly 6–15×, so a single primary is insufficient at full scale. The fix is horizontal: hash `user_id` into N shards (e.g., consistent hashing, N chosen so each shard's write load stays comfortably under its single-primary ceiling — at ~20K writes/sec per shard, N ≈ 9–10 shards covers the peak with headroom), each an independent Postgres primary + its own read replicas. Because rating updates are keyed on individual users and a match only ever touches two specific users' rows, **no cross-shard transaction is required** for the common case — each player's rating update can commit independently against their own shard. (The one place this gets subtle: two players in the same match may land on different shards; since each side's rating update only depends on read-only inputs — both players' *prior* ratings, fetched before the write — this is safely two independent single-shard writes, not a distributed transaction.)
3. **Redis as a read-through cache** in front of Postgres for the hottest reads — session/auth token validation and ELO lookups during matchmaking. This is the same Redis tier proposed in Section 2 for session/room routing, so it's infrastructure we need regardless; using it to cut DB read load is close to free. In particular, the ELO ±100 matchmaking scan is far better served by a Redis **sorted set** keyed by rating (an O(log N) range query in memory) than by a Postgres range query per matchmaking attempt at this request volume.
4. **Partial-failure handling for cross-shard writes.** Point 2 establishes that a match's two rating updates don't need a distributed *transaction*, since each only depends on read-only prior values — but they do need a story for **partial failure**: if player A's shard write succeeds and player B's shard write then fails (network blip, node restart, timeout), naively one rating is updated and the other isn't, which is a real correctness gap in a competitive ranking system. Fix: an **outbox pattern** — when a match completes, the owning game-hosting container durably records *both* intended rating deltas as a single local write (one row/queue entry, written once, idempotently) *before* attempting either shard write, and publishes a `match-completed` event over **NATS**; a separate retrying worker subscribed to that subject then applies each shard write independently — retried until it succeeds — using the outbox entry as the source of truth, so a crash mid-application can safely resume without losing or double-applying either side. In effect, the outbox entry plus the NATS event *is* a **message queue**: it decouples "a match finished, here's the rating delta" (the event, published on NATS) from "actually apply it to Postgres" (the processing, done by the subscribing worker) — the same fire-an-event/durably-process-it split a dedicated broker gives, using the internal event bus we already need for other inter-service notifications (Section 2.1) rather than standing up a separate queueing service.

*See `Server_Design_Rationale_and_Open_Questions.md` for the NoSQL/NewSQL alternative considered for the primary store and the open trade-off around consistency guarantees.*

---

## 2. Concurrency & Routing Architecture

### 2.1 Component breakdown

```
              ┌──────────────────────┐        ┌───────────────────────┐
              │     API Gateway        │        │     WS Gateway          │
              │     (REST/HTTP)        │        │     (WebSocket,         │
              │  login, rooms, history │        │  async I/O, no thread   │
              │  (TLS termination)     │        │  per client)            │
              └───────────┬───────────┘        └────────────┬────────────┘
                          │                                  │
             ┌─────────────┼──────────────┐                   │ (hands the client
             │             │              │                   │  off to its assigned
     ┌───────▼───────┐ ┌───▼────────┐ ┌───▼───────┐            │  container, then
     │  Auth Service  │ │Matchmaking │ │Room Service│            │  gets out of the way)
     │  (stateless)   │ │  Service   │ │(stateless) │            │
     └───────┬────────┘ └─────┬──────┘ └─────┬──────┘            │
             │                │              │                  │
             │        ┌───────▼──────────────▼──────┐            │
             └───────►│   Redis: Session / Room       │◄──────────┘
                      │   Directory                    │
                      │  (game_id → container)         │
                      │  (rating sorted set)            │
                      └────────────────┬────────────────┘
                                       │
                              ┌────────▼─────────┐
                              │  Game Allocator    │  (picks a container with
                              │                     │   spare capacity for a new
                              └────────┬────────────┘  match/room; writes the
                                       │                assignment into the Directory)
              ┌─────────────────────────┼─────────────────────────┐
              │                         │                         │
      ┌───────▼────────┐       ┌────────▼───────┐        ┌────────▼───────┐
      │ Game-hosting    │       │ Game-hosting    │        │ Game-hosting    │
      │ container A     │       │ container B     │  ...   │ container N     │
      │ (GameEngine +   │       │ (GameEngine +   │        │ (GameEngine +   │
      │ RealTimeArbiter │       │ RealTimeArbiter │        │ RealTimeArbiter │
      │  per room)      │       │  per room)      │        │  per room)      │
      └───────┬────────┘       └────────┬────────┘        └────────┬────────┘
              │                         │                          │
              └──────────── NATS: match-completed events ──────────┘
                                       │
                              ┌────────▼─────────┐
                              │  PostgreSQL       │
                              │  (sharded, + read │
                              │   replicas)        │
                              └───────────────────┘
```

- **API Gateway** — public entry point for everything that isn't a live game session: TLS termination, routes HTTP(S) requests (login, matchmaking, room create/join, history) to the stateless Auth/Matchmaking/Room services.
- **WS Gateway** — separate from the API Gateway on purpose: it only ever handles the *handoff* — authenticating the upgrade and pointing the client at the game-hosting container the Directory says it belongs to — then gets out of the way, since the client connects to that container directly (Section 2.3). Splitting REST and WebSocket into two services matters because they scale on different axes: the API Gateway scales with request *rate* (short-lived, stateless HTTP calls), while the WS Gateway scales with *connection count* (long-lived, async-I/O sockets, no thread per client) — conflating them would force one scaling policy to serve two very different load shapes.
- **Auth Service** — stateless; validates credentials against the sharded Postgres store (via Redis cache), issues a session token.
- **Matchmaking Service** — implements the ELO ±100 band search using the Redis rating sorted set; stateless itself, but reads/writes shared queue state in Redis so any replica of this service sees the same queue (this is what lets us run *many* instances of the Matchmaking Service behind the Gateway rather than one).
- **Room Service** — implements the "Rooms" path: create a named room, join by name, additional joiners become spectators. Also stateless; all durable room state lives in the Directory.
- **Redis: Session / Room Directory** — the answer to *"how do we know which server a player is on?"* A single logical Redis deployment (itself typically run as a small clustered/replicated Redis for its own availability, but conceptually "one directory") holding `game_id/room_id → (container address, state, participant list)`. This is the one piece of shared, strongly-visible state every layer consults, and it's small (a handful of fields per active room, not per user), so it stays fast even at 10M-concurrent scale.
- **Game Allocator** — the component that actually makes the "which container gets this match/room" decision, referenced throughout Section 2.2: it performs the atomic capacity-checked selection against the Directory's per-container capacity counters and writes the resulting `game_id → container` mapping. Pulling it out as its own named component (rather than leaving the logic implicit inside Matchmaking/Room Service) keeps that one piece of scheduling logic in one place, shared by both the "Play" and "Rooms" paths instead of duplicated across them.
- **Game-hosting containers** — this is the direct evolution of what already exists today: the in-process `game_id → GameSession` map, `GameEngine`, `RealTimeArbiter`, and the mutex-guarded pub/sub bus. At scale, that map is **partitioned across many containers** instead of living in one process; each container owns some number of rooms end-to-end (owns the `Board`, is the sole mutator via `RealTimeArbiter`, runs the pub/sub bus for its own rooms' spectators). See Section 4 for how many rooms per container and why.
- **PostgreSQL (sharded + replicas)** — as designed in Section 1. Game-hosting containers write match results asynchronously (not blocking the single-threaded game loop on a DB round-trip) — conceptually the existing in-process pub/sub bus's "score update" callback becomes a **NATS** publish to a persistence-writer worker instead of an in-process callback, but the *pattern* — game logic fires an event, something else durably records it — is unchanged from today. NATS (or Redis Pub/Sub) is the internal event bus used wherever a service needs to notify another without blocking on it — the same role it plays for the outbox delivery described in Section 1.3, point 4.

### 2.2 Request flow

**"Play" path (ELO matchmaking):**
1. Client authenticates via the API Gateway → Auth Service → gets a session token.
2. Client requests matchmaking; API Gateway routes to a Matchmaking Service instance.
3. That instance inserts the player into the Redis-backed rating-sorted queue and scans for an opponent within ±100. Because multiple Matchmaking Service instances scan and mutate this same shared queue concurrently, **the match-and-remove step must be a single atomic operation** — e.g. a server-side Lua script, or a `ZPOPMIN`-style atomic Redis primitive — that finds a candidate and removes *both* matched players from the queue in one indivisible step. Without this, two service instances could race and match the same waiting player into two different games at once. On match:
   - Selects a game-hosting container with spare capacity — this selection-and-reservation must itself be atomic with respect to capacity (e.g. an atomic decrement, `DECR`/Lua script, against a per-container "available capacity" counter in the Directory), so a container's slot is claimed exactly once per match and the assigning instance falls back to a different container if the decrement would take it below zero, otherwise two instances could both see spare capacity on the same container and overshoot it,
   - Writes `game_id → container address` into the Directory,
   - Returns connection info to both clients.
4. Both clients open a WebSocket connection directly to that specific container (Section 2.3); from here, every move for that game flows only through the one container that owns it, exactly like the single-server design today, just narrowed to "this one room."

**"Rooms" path:**
1. Creator calls the Room Service, which allocates a game-hosting container and registers `room_id/name → container` in the Directory, marks it "waiting." Room-name reservation must be an **atomic conditional write** (e.g. Redis `SETNX` — set-if-not-exists) so that if two Room Service instances process "create room" requests with the same name at nearly the same moment, exactly one wins the Directory entry and the other gets back a clear "name already taken" error, rather than the two silently racing to overwrite the same key.
2. A second client joins by room name; Room Service does a Directory lookup (a single Redis `GET`, O(1), globally consistent since the Directory is one logical store every Room/Matchmaking instance shares) and routes them to the same container as the creator.
3. Further joiners connect as spectators, same lookup, same container. The container's existing pub/sub bus — unchanged in kind, just now serving however many sockets are attached to that room — fans out board/move events to all of them.

### 2.3 How a client gets to the right container

The API Gateway/Matchmaking/Room Service/Game Allocator only ever perform the *lookup and assignment*; the client is handed the game-hosting container's address (via the WS Gateway, for the WebSocket upgrade) and connects to it directly, bypassing the Gateways for the lifetime of the match — chosen for latency, since this is the pattern most latency-sensitive multiplayer game backends use (matchmaker hands out a game-server IP:port, client connects directly), and it avoids concentrating all 10M connections on the Gateway tier or adding an extra network hop to every move. The cost is that it requires each game-hosting container to be independently network-reachable (e.g., its own externally routable address/port), which is a Kubernetes networking/service-exposure detail — explicitly deferred here since it's part of the Kubernetes learning this document isn't trying to front-run.

*See `Server_Design_Rationale_and_Open_Questions.md` for the full comparison against gateway-proxied routing.*

**Authorization for direct-connect.** Direct-connect raises a separate, application-layer question, distinct from the networking-reachability question just deferred: reachability alone doesn't stop a client from connecting to a container for a room it isn't part of, or from scanning/guessing `game_id`s. Fix: the Matchmaking/Room Service issues a short-lived, single-use **session ticket** — a signed token (e.g. HMAC-signed, short expiry) bound to the specific `game_id` and `player_id` — as part of the "connection info" already handed to the client in Section 2.2. The game-hosting container validates that ticket before completing the WebSocket handshake for that room, and rejects the connection if the ticket is missing, expired, or doesn't match the room/player it's presented for. To stop the same ticket being replayed for a second connection within its validity window (e.g. a leaked/observed ticket used from a second device), the container marks the ticket consumed on first successful validation (a short-TTL Redis key set at that point) and rejects any later attempt presenting the same ticket, even if it hasn't yet expired. This lives entirely in the application layer and doesn't depend on how the Kubernetes networking/exposure question above is eventually resolved.

### 2.4 Why this preserves "anyone can join any room" without cross-container sync

The hard part of a distributed real-time game is usually keeping shared mutable state consistent across machines. This design sidesteps that problem entirely for gameplay: **a room's board state only ever exists on the one container that owns it**, for the room's entire lifetime — nothing about `RealTimeArbiter`'s "only mutator of `Board`" invariant changes, it just now applies per-container instead of per-process-wide. The only thing that needs to be visible *across* containers is the small, low-churn "who owns which room" mapping in the Directory — a bookkeeping problem, not a state-synchronization problem. That's precisely why Redis (fast, centralized, simple key-value semantics) is sufficient here, instead of needing a distributed-consensus system: the Directory doesn't need to agree on game state, only on room ownership.

### 2.5 DB-backed alternative for the matchmaking queue

This is a DB-native alternative to the Redis sorted-set matchmaking design in Section 1.3, point 3 and Section 2.2, implemented via range partitioning rather than a full-table scan.

**1. Range-partition the queue table by rating band.**

```sql
CREATE TABLE matchmaking_queue (
    user_id    BIGINT      NOT NULL,
    rating     INTEGER     NOT NULL,
    status     TEXT        NOT NULL DEFAULT 'waiting',
    queued_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (rating, user_id)
) PARTITION BY RANGE (rating);

CREATE TABLE matchmaking_queue_0800_1000 PARTITION OF matchmaking_queue
    FOR VALUES FROM (800)  TO (1000);
CREATE TABLE matchmaking_queue_1000_1200 PARTITION OF matchmaking_queue
    FOR VALUES FROM (1000) TO (1200);
CREATE TABLE matchmaking_queue_1200_1400 PARTITION OF matchmaking_queue
    FOR VALUES FROM (1200) TO (1400);
-- ... one 200-point partition per band, spanning the full rating range
```

A partial index, created once on the parent (Postgres propagates it to every partition automatically), keeps each partition's index limited to rows that actually matter for matchmaking:

```sql
CREATE INDEX idx_matchmaking_waiting ON matchmaking_queue (rating)
    WHERE status = 'waiting';
```

**2. Why this is write sharding, not just "an index."** Every new queue entry lands in exactly one partition — the one whose band contains that player's current rating — because `PARTITION BY RANGE (rating)` routes the insert at the storage layer, not the application layer. A player at rating 1550 always inserts into `matchmaking_queue_1200_1400`; a player at rating 2100 never touches that table or its index at all. This spreads insert load across as many physical tables (and physical B-trees) as there are bands, instead of funneling every queued player through one table and one index — the same load-spreading goal as the `user_id` hash-sharding in Section 1.3, just keyed by `rating` instead of `user_id`, because the thing that needs spreading here is a range-scanned queue rather than a point-looked-up user store.

**Scope of this write sharding: within one primary, not across machines.** All of these partitions live on the same Postgres primary — they share that primary's WAL and the same 10,000–30,000 writes/sec ceiling established in Section 1.3. What this buys is real: each partition's B-tree stays small and fast to update, and insert load is spread across many smaller indexes instead of one large one, cutting index-maintenance cost per insert. What it does not buy is more total write capacity — that requires cross-machine sharding, the kind Section 1.3 uses for `user_id`: N independent Postgres primaries, each with its own write ceiling, so total throughput scales with N. If the matchmaking queue's own write volume ever needed genuine horizontal write scaling (not just per-insert efficiency), that would mean placing each rating-band partition's writes against a different Postgres primary — combining this same partitioning with the `user_id`-style hash-sharding from Section 1.3, keyed instead by rating band. That combination isn't designed here; it's flagged so a reader doesn't assume this section alone solves cross-machine write scaling for the queue.

**3. Why this makes the ±100 read efficient — partition pruning.** A matchmaking search for a player at rating R issues:

```sql
SELECT user_id, rating
FROM matchmaking_queue
WHERE status = 'waiting'
  AND rating BETWEEN 1450 AND 1650;
```

With 200-point bands, `[R-100, R+100]` is 200 points wide, so it overlaps **at most two adjacent partitions** (one when the band happens to fall entirely inside a single partition, two when it straddles a boundary). Postgres's planner evaluates the partition bounds against the `WHERE rating BETWEEN ...` clause at plan time and prunes every non-overlapping partition from the query entirely — it never opens their files, never touches their indexes. The B-tree actually scanned is therefore the size of one or two rating bands' worth of waiting players, not the whole system's queue, regardless of how many total players (up to 10M, at peak) are queued system-wide. This is the direct read-side payoff of the write-side partitioning in point 2 — the same partitions that spread inserts are what let a range scan skip almost everything.

**4. Atomic claim: `SELECT ... FOR UPDATE SKIP LOCKED`, the Postgres-native equivalent of the Redis Lua/`ZPOPMIN` claim.** Section 2.2 requires that two Matchmaking Service instances can never both claim the same waiting player. The DB-native equivalent of that atomic claim is `SELECT ... FOR UPDATE SKIP LOCKED`, scoped to the same pruned partition range, inside one transaction with the status update that marks the row taken:

```sql
BEGIN;

SELECT user_id
FROM matchmaking_queue
WHERE status = 'waiting'
  AND rating BETWEEN 1450 AND 1650
  AND user_id <> 48213077              -- exclude the searching player
ORDER BY abs(rating - 1550)            -- closest rating first
LIMIT 1
FOR UPDATE SKIP LOCKED;

-- application reads back candidate_user_id from the row above, then:
UPDATE matchmaking_queue
SET status = 'matched'
WHERE user_id = $candidate_user_id
  AND rating BETWEEN 1450 AND 1650
  AND status = 'waiting';

COMMIT;
```

`FOR UPDATE` takes a row lock on the candidate; `SKIP LOCKED` tells Postgres that if a concurrent Matchmaking Service instance already holds a lock on that row (because it is mid-claim on the same candidate), skip it and consider the next-best candidate instead of blocking. Two instances racing on the same partition therefore never both walk away thinking they claimed the same player — one gets the row, the other's `SELECT` transparently skips past it to a different waiting player (or finds none, and the searching player stays queued for the next scan). This is the same "claim without double-booking" guarantee as the Redis Lua/`ZPOPMIN` script in Section 2.2, expressed with Postgres's own concurrency primitives instead of a scripted atomic Redis operation.

**5. Implementation notes.** The partial index (`WHERE status = 'waiting'`) from point 1 keeps each partition's index limited to currently-waiting players only, not the full history of matched/expired rows — matched/expired rows need periodic deletion (a queue is transient state, not a permanent record, unlike the rating history it feeds into via the outbox in Section 1.3, point 4). Each partition can also be created `UNLOGGED` directly — `CREATE UNLOGGED TABLE matchmaking_queue_1200_1400 PARTITION OF matchmaking_queue FOR VALUES FROM (1200) TO (1400);` — since losing an in-flight queue entry on a crash is low-severity: the affected player simply isn't matched and re-queues, unlike a lost rating write, which Section 1.3's outbox pattern exists specifically to prevent.

*See `Server_Design_Rationale_and_Open_Questions.md` for the naive full-table-scan version this design avoids, and the latency trade-off against the Redis approach.*

### 2.6 Observability

Every other section of this design assumes the system's behavior is actually visible while it's running — capacity-aware container selection (2.2), liveness-based failure detection (5.1), and replica health-checking (5.2) all depend on it. This section names Observability as its own always-on component, alongside Auth/Matchmaking/Room Service, rather than leaving it implicit:

- **Logs** — structured, per-service logs (already the pattern in today's single-process server's `server.log`), shipped off-container so they survive the container itself dying (relevant given Section 4's short-lived game-hosting containers).
- **Metrics** — per-service and per-container: request rate and latency (Gateways), queue depth and match rate (Matchmaking), messages/sec and room count (game-hosting containers, directly feeding the capacity math in Section 3.4/4.3), write latency and replication lag (Postgres). These are what an autoscaler (Section 4.4) actually scales on.
- **Alerts** — threshold-based on the metrics above: e.g. a shard's replication lag exceeding the read-your-writes window (Section 1.3, point 1), or a container's liveness key expiring unexpectedly (Section 5.1).
- **Load tests** — the numbers in Section 3 (traffic) and Section 1.1 (write throughput) are calculated from the assignment's stated parameters, not measured; load testing is what validates them against a real deployment before trusting the design at actual scale, and is what the per-core message-processing estimate in Section 3.4 should eventually be checked against.

This component doesn't sit in the request path of any client action — it's a passive consumer wired to every other component, not a dependency any of them wait on.

### 2.7 Service Discovery

Sections 2.2–2.3 already depend on the Game Allocator and Directory knowing which game-hosting containers exist and are healthy, but that dependency has been implicit until now — naming it explicitly:

Kubernetes provides this at the infrastructure level: when a game-hosting container starts and passes its readiness probe, it becomes reachable as an endpoint within the cluster network, without any service embedding a fixed container IP/hostname. The Game Allocator queries this discovery layer (or a registry backed by it) to get the current set of *healthy* containers before running the capacity-checked selection in Section 2.2 — this is a distinct signal from the Directory's `game_id → container` entries (Section 2.1): discovery answers "which containers exist and are alive," the Directory answers "which room is on which container," and the per-container capacity counters answer "how loaded is it." A container that fails its health check is removed from discovery and stops receiving new-room assignments, independent of and prior to the liveness-key/fencing-token mechanism in Section 5.1, which handles the stricter case of a container that's already mid-match. As with the rest of the Kubernetes layer, the specific discovery mechanism (kube-dns, a service mesh, etc.) is left conceptual, consistent with Section 4.4.

### 2.8 Game Lifecycle — End-to-End Summary

The steps below are a roadmap through the "Play" path already detailed in Sections 2.2–2.3 and 5, given here as one ordered list so the full lifecycle of a single match is visible in one place:

1. Client connects to the API Gateway and authenticates (Section 2.2, Auth Service; Section 1).
2. Client requests matchmaking via the API Gateway (Section 2.2).
3. Matchmaking Service atomically claims a matching opponent from the Redis rating queue (Section 2.2).
4. Game Allocator selects a game-hosting container with spare capacity, from the set of containers Service Discovery reports healthy (Section 2.7), and reserves it atomically (Section 2.2).
5. `game_id → container` is written to the Directory; a signed session ticket is issued to both clients (Section 2.3).
6. Both clients connect directly to the assigned container via the WS Gateway handoff, presenting their session ticket (Section 2.1, 2.3).
7. The container's `RealTimeArbiter` runs the match, broadcasting moves to the opponent (and any spectators) for the match's 30–90s duration.
8. On completion, the container writes an outbox entry and publishes a `match-completed` event over NATS (Section 1.3, point 4).
9. A retrying worker applies the rating-delta writes to the appropriate Postgres shard(s) (Section 1.3, point 4).
10. The Directory's `game_id → container` entry is released; both clients return to the lobby.

If a container dies mid-match instead of completing normally, steps 7–10 are replaced by the void-the-match path in Section 5.1.

---

## 3. Network Traffic Analysis

### 3.1 Message rate

Given: 10,000,000 concurrent players, each making a move roughly every 2 seconds on average.

```
message rate = 10,000,000 players / 2 s = 5,000,000 moves/second (system-wide, inbound)
```

### 3.2 Message size

The protocol already sends structured JSON, not raw pixels — measuring an actual representative payload rather than guessing:

**Client → server (move command):**
```json
{"type":"move","game_id":"7f3a9c21-4b8e-4d2a-9f1c-2b6e7a8d9c10","player_id":48213077,"from":"e2","to":"e4","piece":"P","ts":1737902345123}
```
= **138 bytes** (measured, UTF-8).

**Server → client (move broadcast to the opponent, includes enough for the receiving client to animate — piece, timing, sequence number for ordering):**
```json
{"type":"move_broadcast","game_id":"7f3a9c21-4b8e-4d2a-9f1c-2b6e7a8d9c10","player_id":48213077,"from":"e2","to":"e4","piece":"P","duration_ms":400,"ts":1737902345123,"seq":128}
```
= **176 bytes** (measured, UTF-8).

Add protocol framing overhead:
- **WebSocket framing**: for payloads >125 bytes, a frame header is 4 bytes unmasked (server→client) or 8 bytes masked (client→server frames must be masked per RFC 6455) → client message ≈ 138 + 8 = **146 bytes on the wire**; server message ≈ 176 + 4 = **180 bytes on the wire**.
- **TCP/IP overhead**: baseline IPv4+TCP header is 40 bytes per segment (more with TCP options/timestamps, and TLS adds a further ~20–30 bytes of record overhead for `wss://`, which we should be using). Using the conservative 40-byte baseline: client message ≈ **186 bytes/packet**, server message ≈ **220 bytes/packet**.

These per-packet overheads matter here specifically *because* our payloads are small — a 40-byte header on a 138-byte payload is ~29% overhead, not the rounding error it would be on a bulk transfer. This is a concrete argument for **batching/coalescing** outbound updates when a single container is broadcasting many moves close together (e.g. a short server-side buffer flushed every 10–20ms) as a future optimization — noted here, not designed in depth, since it isn't required to answer the assignment's core question.

### 3.3 Aggregate bandwidth

**Inbound** (every player's move, sent once to their container):
```
Payload only:        5,000,000/s × 138 B  = 690,000,000 B/s  ≈ 690 MB/s  ≈ 5.52 Gbps
With WS+TCP/IP:       5,000,000/s × 186 B  = 930,000,000 B/s  ≈ 930 MB/s  ≈ 7.44 Gbps
```

**Outbound** (base case: 1-on-1 game, no spectators — every move is broadcast to exactly the one opponent; the mover's own client renders optimistically/locally per the existing "smart client" design, so no echo-back is assumed here):
```
Payload only:        5,000,000/s × 176 B  = 880,000,000 B/s  ≈ 880 MB/s  ≈ 7.04 Gbps
With WS+TCP/IP:       5,000,000/s × 220 B  = 1,100,000,000 B/s ≈ 1.10 GB/s ≈ 8.80 Gbps
```
(Spectators, room chat, presence pings, matchmaking traffic, and reconnect/resync traffic all add to this; the numbers above are a floor, not a ceiling, for a game with spectators enabled.)

**Total aggregate, system-wide, at peak:**
```
Payload only:   690 + 880   = 1,570 MB/s  ≈ 12.6 Gbps
With overhead:  930 + 1,100 = 2,030 MB/s  ≈ 16.2 Gbps
```

### 3.4 Is this a lot of traffic?

**For the internet as a whole: no.** Internet backbone and cloud-provider cross-region links operate at multi-Tbps scale; 12–16 Gbps sustained is a rounding error next to that — comparable to a single well-provisioned CDN edge node's traffic, not a "the internet can't handle this" problem. It's important to be precise about *why* this doesn't mean "so one server is fine":

**For a single server: yes, decisively.** Two independent limits bind before "is there enough bandwidth" does:

1. **NIC capacity.** Commodity cloud instance NICs are commonly provisioned at 1–25 Gbps (10/25 GbE is typical for larger instances; higher tiers exist but aren't the default assumption). Our computed 16.2 Gbps *already* saturates or exceeds a 10–25 GbE NIC once we leave headroom for bursts, TLS overhead, and non-gameplay traffic (matchmaking, auth, spectators) — and that's optimistic, assuming perfectly even load, no retransmits, and payload-tight framing.

2. **CPU / message-processing throughput — the tighter constraint.** Bandwidth being available doesn't mean a single process can *do the work* for every message. Critically, "processing a move" isn't just the inbound half: it means deserializing and validating the **inbound** move (`RuleEngine::Validate`, apply via `RealTimeArbiter`) *and* serializing and dispatching the **outbound** broadcast to the opponent — Section 3.2 already treats these as two distinct payloads (138 B in, 176 B out), so the true message-processing volume is inbound *plus* outbound, not inbound alone:
   ```
   total messages processed ≈ 5,000,000/s (inbound) + 5,000,000/s (outbound, 1-on-1 base case)
                             = 10,000,000 messages/sec
   ```
   (Rooms with spectators add further outbound fan-out on top of this base case — every spectator is another broadcast per move — so this is a floor, consistent with the bandwidth floor noted in 3.3.) At a conservative **20–50 microseconds per message** on one CPU core (reasonable for this amount of small-JSON-parse-plus-simple-game-logic work, with no blocking I/O in the hot path):
   ```
   max throughput per core ≈ 1 / 50µs  to  1 / 20µs  =  20,000 – 50,000 messages/sec
   ```
   We need **10,000,000 messages/sec** total. Even before accounting for lock contention on the current mutex-guarded pub/sub bus or connection-count limits, that's:
   ```
   10,000,000 / 20,000..50,000 ≈ 200 – 500 cores' worth of message-processing capacity, minimum
   ```
   And because **today's server is single-threaded** (one IXWebSocket event loop, one core), it can only ever use *one* of those cores — meaning the current architecture, completely unmodified, tops out around 20,000–50,000 total messages/sec, or roughly **0.2%–0.5% of the required capacity**. That is the real, numbers-backed argument for horizontal distribution: not "the internet is too small," but "one process, however fast its NIC, cannot execute anywhere near enough game-logic operations per second, or hold anywhere near 10M sockets" (typical single-host connection ceilings, even with aggressive OS tuning of file descriptors and ephemeral ports, land in the hundreds of thousands to low millions — nowhere close to 10M on one machine either).

This is precisely why Section 2's design shards rooms across many independent game-hosting containers: it isn't primarily a bandwidth argument, it's a **CPU-bound, single-threaded-event-loop-per-container argument**, and it directly motivates the container-sizing calculation in Section 4.

---

## 4. Container Lifecycle & Docker Roles

### 4.1 Two distinct container roles

**Long-lived, always-on services** — Gateway/LB, Auth, Matchmaking, Room Service. These scale with *steady-state* concurrent load (connection count, request rate), not with individual matches; they behave like conventional horizontally-scaled stateless microservices, with a replica count that moves gradually (autoscaled on CPU/connection metrics) rather than churning per-match. Postgres and Redis are run as managed/stateful services with their own lifecycle (replication, failover), not treated as ephemeral compute at all.

**Short-lived, per-match/per-room game-hosting containers** — this is where the 30–90s match duration has a direct, load-bearing consequence.

### 4.2 Container lifecycle model

One container per match is not viable at this scale due to churn rate (~83,333 matches/s at peak, per Section 1.1 — see `Server_Design_Rationale_and_Open_Questions.md` for the full churn-rate calculation and why it rules out per-match containers). Section 4.3 describes the pooled model used instead.

### 4.3 Recommended model: pooled, multi-room containers, sized by measured per-core throughput

Instead, each game-hosting container hosts a **pool of many concurrent rooms**, sized to what its available CPU can actually process — directly reusing the Section 3.4 estimate of ~20,000–50,000 messages/sec per core. At the assignment's move cadence (1 move per 2s per active player, 2 players per game ⇒ ~1 inbound move/s per game; each move also produces 1 outbound broadcast to the opponent in the 1-on-1 base case, so ~2 messages/s total per game, matching the in+out accounting in 3.4), a single core's processing budget of 20,000–50,000 msgs/sec corresponds to **on the order of 10,000–25,000 concurrent games per core** on the message-processing dimension alone (spectator-heavy rooms push this down further, since each spectator adds another outbound broadcast per move) — still comfortably enough that, in practice, a game-hosting container will run out of memory or connection-count headroom long before it runs out of per-core message-processing budget. This means container *sizing* should be driven by memory/connection limits, not by the CPU-per-message math directly — but the CPU math is what proves that pooling is viable at all (there's plenty of headroom per core to host many simultaneous slow-cadence games).

With pooling:
- Individual **rooms** still churn at the same ~83,333/s rate (Section 1.1) — but that churn is now just "a room object created/destroyed inside an already-running container," identical in kind to what `GameSession` creation/teardown looks like in the codebase today, not a container lifecycle event.
- **Containers themselves** are recycled on a much coarser timescale (minutes, not seconds) — driven by memory growth, rolling deploys, and load rebalancing, not by individual match completion. This reconciles the extreme *room*-churn rate with a realistic *container* churn rate.
- The Directory (Redis) still tracks ownership at the fine grain of individual `game_id`s, even though many `game_id`s map to the same container — this is a natural generalization of the existing in-process `game_id → GameSession` map: it's the same map, just horizontally partitioned, with each container's slice of it sized to what that container is provisioned to hold.

### 4.4 Fit with Kubernetes/K3s (conceptual only)

- **Always-on services** (Gateway, Auth, Matchmaking, Room Service) map cleanly onto standard Kubernetes **Deployments** behind a **Service**/**Ingress** — this is the conventional, well-trodden use case and needs no special treatment.
- **Game-hosting containers** are semi-stateful for their lifetime (they hold live `Board`/`RealTimeArbiter` state in memory for every room they're currently hosting, with no external DB dependency mid-match) — this is a better fit for a **Horizontal Pod Autoscaler** driven by aggregate connection count or CPU (scaling the *pool* of containers up/down as total concurrent-room count changes, e.g. across time-zone-driven diurnal cycles worldwide) than for per-match provisioning. Pod termination requires a graceful-drain step, so a container being scaled down isn't killed while it still holds live rooms.
- This two-tier split — steady, autoscaled Deployments for stateless services, and a semi-stateful, connection-aware autoscaled pool for game-hosting — is a good conceptual fit for K8s/K3s's autoscaling and fast pod lifecycle primitives, without requiring a fully custom scheduler.

*See `Server_Design_Rationale_and_Open_Questions.md` for the container-lifecycle-model trade-off and open Kubernetes-mechanism questions.*

---

## 5. Failure Recovery — Action Plan

The assignment explicitly calls for a plan covering server and database failure, distinct from the client-side disconnect handling already in place (the existing 20-second auto-resign grace period, unaffected by anything below). This section covers the two failure modes that fall directly out of the architecture in Sections 1–4: a game-hosting container going down mid-match, and a Postgres node going down.

### 5.1 Game-hosting container failure (mid-match)

**1. Detection.** Every game-hosting container renews a short-TTL liveness key in the Directory (Redis) on a fixed heartbeat — e.g. refreshed every 2–3s against a 10s TTL. This is a second, separate signal from the per-room `game_id → container` mapping the Directory already holds (Section 2.1): one entry says *where* a room lives, the other says *whether that container is still alive at all*. If a container's liveness key expires without renewal, a lightweight monitor (the orchestrator's own health-check, or a small reaper process watching the Directory) declares it dead.

A missed heartbeat isn't proof the container is actually dead — because clients connect to game-hosting containers directly (Section 2.3), a container can suffer a **transient network blip to Redis alone** while remaining fully alive and still serving its already-connected players; if it reconnects after being declared dead and reassigned, it could otherwise try to keep acting as owner of capacity/rooms already handed to its replacement — a false-positive, split-brain case distinct from a true crash. The fix is a **fencing token**: each time a container (re-)registers as owner of a liveness key or capacity slot, the Directory issues it a monotonically increasing generation number (e.g. via Redis `INCR`), which the container must attach to every subsequent liveness renewal and every write it makes (releasing capacity, updating room ownership). Once a container is declared dead and its slot reassigned, its generation number is stale, so the Directory rejects any further write tagged with it — a "resurrected" container can observe that it's been fenced out but can no longer reclaim ownership out from under its replacement. This addition only guards the false-positive case; the true-crash handling below is unaffected.

**2. Immediate handling of in-flight matches.** Every room hosted on that container loses its in-memory `Board`/`RealTimeArbiter` state the moment the container dies — there is no replicated in-memory state to fail over to. The design's response is deliberately simple: **void every match that was in-flight on that container**, rather than attempt state reconstruction. This is a conscious simplicity-over-continuity trade-off, justified by the same fact that shapes Section 4's container-lifecycle design: matches last 30–90 seconds, so the number of matches exposed to any one container's death at any moment is small, and each one's loss is cheap to the player (a short match, not hours of progress). Concretely, "void" means: both players receive a "connection lost — no result recorded" notice; **no rating change is applied to either player** (no outbox entry is ever written for that match, since it never reached the completion path that creates one — see Section 1.3, point 4); and both players are immediately free to re-queue for matchmaking or re-create/rejoin a room, with no lockout.

**3. Directory cleanup.** Once a container is declared dead, every `game_id → container` entry pointing at it is deleted (or flagged stale) from the Directory, so no future matchmaking/room lookup can ever route a client to a dead address. This must also release that container's reserved capacity: the atomic per-container capacity counter introduced in Section 2.2 needs to be reset (or the container removed from the assignable pool entirely) — otherwise a dead container's "slots" would sit permanently reserved-but-unusable, quietly shrinking total system capacity every time a container fails.

**4. Replacement capacity.** No special failover path is needed for *new* matches — they're simply assigned to other live containers by the existing selection-and-reservation mechanism (Section 2.2), unchanged. The one requirement this places on capacity planning: the container pool must run with standing headroom (provisioned for N+k containers' worth of capacity, not exactly N), so that one container's failure reduces available headroom rather than causing new-match assignment to fail outright.

### 5.2 Database (Postgres) node failure

Section 1.3's design already gives each shard a primary plus multiple read replicas, so "a Postgres node fails" splits into two very different-severity cases.

**1. Replica failure — the easy case.** A failed read replica is simply health-checked out of the read-routing pool; reads for that shard continue against its remaining replicas. No write path is affected, no failover logic is needed beyond removing (and later re-adding) the dead replica from rotation.

**2. Shard primary failure — the case that needs a plan.** Losing a shard's primary is a standard Postgres failover: one of that shard's read replicas is promoted to primary — either automatically, via a failover-management tool/operator, or manually by an on-call operator, depending on operational maturity at the time (this document doesn't design that tooling, only requires that promotion happens) — and the shard's "current primary" address, a small piece of routing state held alongside the Directory, is updated so the connection layer starts sending that shard's writes to the new primary. The gap that matters is the **promotion window** — the brief span between the old primary failing and the new one being ready to accept writes — during which any write targeting that shard fails or times out. This is exactly the case the **outbox pattern** from Section 1.3, point 4 already exists to cover: the rating-update intent was durably recorded *before* the shard write was ever attempted, so a write that fails or times out during the promotion window is simply retried by the same retrying worker once the new primary is live, rather than being lost. The outbox isn't only a cross-shard-atomicity mechanism — it's also this system's answer to "what happens to a write that was in flight during a database failover," and that connection is the payoff of having designed it that way in Section 1.3.

**3. Directory (Redis) failure.** One piece of shared infrastructure doesn't yet have a failure story of its own: the Directory. Since every routing decision in Section 2 (matchmaking, room lookup, container liveness, capacity reservation) depends on it, it cannot be a single point of failure. The recommended treatment is the same pattern already applied to Postgres: run the Directory as a small replicated/clustered Redis deployment (e.g. Redis Sentinel or Redis Cluster) with automatic failover, rather than a single instance — the same shape of solution as 5.2's database case, not a new mechanism.

---

*See `Server_Design_Rationale_and_Open_Questions.md` for alternatives considered, acknowledged trade-offs, and open questions across every section above — including the void-on-crash vs. state-checkpointing trade-off in this section, and a note on estimate confidence for the numbers in Sections 1 and 3.*

---

## 6. Out of Scope

The following are deliberately excluded from this design document — either because they belong to the Kubernetes/K3s layer this document treats as conceptual (Section 4.4), or because they're implementation rather than architecture:

- Wire protocol / message schema definitions (beyond the illustrative payloads measured in Section 3.2)
- Postgres/Redis schema DDL beyond the illustrative examples in Sections 1 and 2.5
- CI/CD pipeline design
- Cloud-provider-specific managed services (managed load balancer products, CDN, WAF)
- Anti-DDoS infrastructure
- In-game chat systems
- Billing/payment systems
- Source code / implementation-level details

Observability (Section 2.6) and the message-queue mechanism (NATS, Section 1.3/2.1) are explicitly **in scope**, unlike in some comparable designs, since both were called out as required by the follow-up architecture guidance.
