# Docker Compose containerization — write-up

This is the smallest useful containerization step for Kung Fu Chess's
server, built toward the target architecture in `Server_Design.md`
without attempting to build all of it at once. Full task-by-task detail
is in `docs/tasks/docker-compose-plan.md`; this document is the summary
for submission.

## What was built

Three Compose services:

- **`kungfu_server`** — the existing single-process C++ server
  (`server/Dockerfile`, multi-stage build), containerized as-is. No
  application code changes were needed: its file paths (SQLite DB,
  board data, log file) were already relative to the process's working
  directory, so a container `WORKDIR` that mirrors that layout is
  sufficient.
- **`postgres`** (`postgres:16-alpine`) — a single, unsharded instance,
  seeded on first startup with a `users` and `matches` schema
  (`postgres/init.sql`) reconciled against `Server_Design.md` Section
  1.3. Nothing in `kungfu_server` reads or writes it.
- **`redis`** (`redis:7-alpine`) — a single instance. Nothing reads or
  writes it either.

All three have a healthcheck (`pg_isready`, `redis-cli ping`, and a
basic TCP-port-open check for `kungfu_server`), visible via
`docker compose ps`. There is no `depends_on` between any of them: since
`kungfu_server` doesn't actually use Postgres or Redis yet, adding one
would misrepresent a coupling that doesn't exist.

## What this proves

- The server, previously only ever built and run on Windows/MSYS2,
  compiles and runs correctly under a Linux toolchain and container —
  this had never been verified before.
- Docker Compose can orchestrate multiple independent services (compute
  + datastore + cache) with a single command, with persistent named
  volumes for the stateful ones. SQLite data was confirmed to survive a
  `docker compose down && docker compose up` cycle.
- The Compose topology's shape reflects `Server_Design.md`'s direction
  (a separate datastore/cache tier from the compute tier), even though
  the wiring between them isn't real yet.
- Postgres starts up with a real schema in place, derived from
  `Server_Design.md` rather than invented independently of it (see
  `postgres/init.sql`'s header comment for exactly which parts are a
  direct reconciliation vs. an explicitly-flagged extrapolation).

## What this does NOT prove

- **No horizontal scaling.** `kungfu_server` is still a single process
  with a single-threaded event loop; running more than one replica
  would have two instances bound to the same port with no shared state
  or Directory between them, so it must stay at one instance.
- **No actual Postgres or Redis integration.** Both are idle containers.
  Postgres has a schema; nothing populates or queries it.
- **No Gateway/Auth/Matchmaking/Allocator decomposition.** Everything
  from `Server_Design.md`'s concurrency/routing architecture (Section
  2) still lives inside the one `kungfu_server` binary.
- **No graceful shutdown, and a real (if narrow) data-safety
  consequence follows from that.** `main.cpp` has no SIGTERM/SIGINT
  handler. `docker compose down` sends SIGTERM, waits Compose's default
  ~10-second grace period, then SIGKILLs the process. If a write to
  `data/kungfu_chess.db` is in flight at that exact moment — a rating
  update or a new-user registration mid-transaction — the process can
  be killed before SQLite finishes committing it, which can leave an
  open WAL file or, in the worst case, a corrupted database on next
  startup. This was not hit during verification (no writes were
  in-flight at shutdown time in testing), but it is a real, stated risk
  of this container's current shutdown behavior, not a hidden one. The
  fix — a signal handler that flushes and closes the SQLite handle
  before the process exits — is a documented follow-up, not part of
  this step.
- **The `kungfu_server` healthcheck only proves TCP port 9002 is
  accepting connections.** It says nothing about whether the
  websocketpp/Asio event loop is actually still processing messages; a
  hung-but-still-listening process would still report healthy.
- **No TLS.** The server speaks plain `ws://`, matching the existing
  code, not `wss://`.
- **No throughput/capacity validation.** Nothing here tests any of
  `Server_Design.md`'s numeric claims (166,667 writes/sec, 10M
  concurrent players, etc.) — this is a structural/orchestration proof
  at a scale of one instance each.

## Verification performed

- `docker build -f server/Dockerfile -t kungfu_server .` — succeeds
  from repo root (image ~118MB).
- Standalone `docker run` smoke test — two scripted WebSocket clients
  registered, logged in, were matched via quick-match (ELO-band
  matchmaking), and one played a real move (`WPe2e4`); the broadcast
  state confirmed the pawn's real-time travel animation completed and
  landed on e4 for both connected clients.
- `docker compose up` — all three services reach `healthy`.
- The same two-client smoke test, repeated against the Compose-managed
  server — passed identically.
- `docker compose down && docker compose up` — the two registered
  users (with their SQLite row data intact) were still present
  afterward, confirming the named volume persists data across restarts.
- `docker compose exec postgres psql -c '\dt'` — shows `users` and
  `matches`, created by `init.sql`.
- `docker compose exec redis redis-cli ping` — returns `PONG`.

## Follow-ups (explicitly out of scope for this step)

See `docs/tasks/docker-compose-plan.md` (T9) for the full list:
SQLite→Postgres migration, real Redis wiring, Gateway/WS-Gateway split,
signal-handler-based graceful shutdown, env-var config plumbing, and
real `depends_on`/health-gated startup ordering once there's an actual
runtime coupling to express.
