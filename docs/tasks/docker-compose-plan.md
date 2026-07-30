# Docker Compose containerization — small working version

## Context

This is a deliverable for a cloud-scaling assignment. `Server_Design.md`
(repo root) describes a *target-state* architecture sized for 100M users /
10M concurrent players: API Gateway + WS Gateway, stateless Auth/
Matchmaking/Room services, a Game Allocator, a Redis-backed Session/Room
Directory, sharded+replicated PostgreSQL, a NATS event bus, and an
Observability stack. It is explicitly a system-design document, not an
implementation plan — it contains no phased/MVP rollout guidance, so the
smallest useful Docker Compose step has to be derived, not looked up (see
its own Section 6, "Out of Scope").

The actual codebase today is the opposite extreme: a single-process,
single-thread C++ binary (`kungfu_server`, built via `server/CMakeLists.txt`
+ CMake/Ninja, historically only ever built under MSYS2/ucrt64 on Windows)
that does WebSocket handling (websocketpp + standalone Asio, no TLS), auth
(SQLite via the raw C API + SHA-256/salt), matchmaking, rooms, and
reconnect/resign — all in-process, all in one SQLite file, with no env
vars, no CLI args, and no existing Dockerfile/Compose anywhere in the repo.

The assignment wants "something small that works" over "everything from
`Server_Design.md` half-built." This step:

- **Keeps `kungfu_server` as one monolithic container** — no Gateway/Auth/
  Matchmaking split yet.
- **Stands up Postgres but does not migrate to it** — SQLite stays the real
  datastore; Postgres is scaffolding for a documented follow-up.
- **Stands up Redis but wires nothing to it** — same reasoning.

The goal is to prove the existing monolith containerizes cleanly and that
Compose can orchestrate it alongside the infra pieces `Server_Design.md`
eventually needs — while being explicit in the write-up about what's real
vs. simulated at this scale.

---

## Key simplification: no C++ changes are required

`server/main.cpp` hardcodes port `9002` and relies on `WebSocketServer`'s
default constructor args, `"data/kungfu_chess.db"` and `"server.log"` —
both **relative to the process's working directory**, not absolute. It also
reads `boards/standard.txt` (also CWD-relative) the first time a session is
created. There are no env var or CLI reads anywhere in `server/`.

Because all of this is CWD-relative rather than hardcoded to an absolute
path, a container whose `WORKDIR` mirrors the same relative layout
(`/app/boards/standard.txt`, `/app/data/`) needs **no source changes at
all** to run correctly — just a Dockerfile that copies things into the
right relative places. This keeps the deliverable's code-risk at zero: the
only new artifacts are `server/Dockerfile`, `.dockerignore`, and
`docker-compose.yml`.

(Graceful SIGTERM handling, env-var config for port/db-path, and an actual
HTTP/TCP health-check endpoint don't exist today either — these are called
out as explicit follow-ups below, not silently assumed in scope.)

The additions below (healthchecks, `init.sql`, an expanded `.dockerignore`)
are all infra-only and don't change this — still zero C++ source edits.

---

## Component split: what's a container now vs. deferred

| `Server_Design.md` component | This step | Reason |
|---|---|---|
| Game-hosting container (`GameEngine`/`RealTimeArbiter`/WS handling) | **Yes** — `kungfu_server` container | Already exists as one binary; containerizing it is the actual assignment task |
| PostgreSQL (sharded + replicas) | **Yes, single unsharded instance, unused, schema-seeded via `init.sql`** | Stood up per the design's direction; schema now mirrors Section 1.3's user-row shape, but no app code reads/writes it yet — real migration is a separate, riskier body of work (see below) |
| Redis (Session/Room Directory) | **Yes, single instance, unused** | Same — stood up as scaffolding, nothing reads/writes it yet |
| API Gateway / WS Gateway | **No** | Would require actually splitting connection-handoff logic out of `WebSocketServer` — out of scope for a "small working" step |
| Auth / Matchmaking / Room Service (as separate stateless services) | **No** | Still in-process inside `kungfu_server`; splitting these needs the Directory to actually be wired first, which isn't happening this step |
| Game Allocator | **No** | Meaningless with only one game-hosting container instance |
| Service Discovery | **No** | Kubernetes-provided in the target design; not applicable to a 3-container Compose file |
| NATS event bus | **No** | Nothing produces or consumes events yet; adding it would be an empty container with no rationale even as scaffolding |
| Observability | **No** | Separate, legitimately large piece of work; not attempted here |

---

## PostgreSQL: what's in scope

**In scope now:** a `postgres` service in Compose (official image, one
instance, no sharding, no replicas), reachable on the network, with a named
volume for data persistence, plus a `pg_isready`-based healthcheck and a
schema seeded on first startup via `init.sql` (mounted read-only into
`/docker-entrypoint-initdb.d/`, Postgres's official images run any `.sql`
file found there exactly once, on an empty data directory). Nothing in
`kungfu_server` connects to it or writes through it.

**`init.sql` schema — reconciled against `Server_Design.md` Section 1.3,
not invented from scratch:**
- **`users` table** — directly reconciled: Section 1.3 describes the row
  informally as "essentially `(user_id, username, password_hash, rating,
  ...)`"; this plan fills in the `...` using the columns that already exist
  concretely in `UserRepository`'s current SQLite schema
  (`id`, `username`, `password_hash`, `password_salt`, `rating`,
  `created_at`), translated to Postgres types/idioms (e.g. `BIGSERIAL`
  instead of SQLite's `INTEGER PRIMARY KEY AUTOINCREMENT`). This is a
  translation of an existing, documented shape — not a new design.
- **`matches` table** — **flagged as an extrapolation, not a direct
  transcription.** `Server_Design.md` never gives DDL for a match-history
  table anywhere — the *only* DDL in the whole document is for
  `matchmaking_queue` (Section 2.5), which is a transient queue table for a
  *different, explicitly-alternative* design (the doc's actual
  recommendation for matchmaking state is the Redis sorted set in Section
  1.3 point 3, not this Postgres table — see below). So `init.sql` will
  **not** include `matchmaking_queue`; including it here would misrepresent
  an alternative as the chosen design. The `matches` table instead derives
  its columns from what Section 1.3 point 4 already describes
  operationally — "durably records *both* intended rating deltas" for the
  two players in a completed match — giving: `id`, `player_white_id`,
  `player_black_id`, `winner_id` (nullable, null = draw), `rating_delta_white`,
  `rating_delta_black`, `completed_at`. This is a reasonable minimal
  complement to a ratings system and echoes the doc's own wording, but it
  is genuinely new schema beyond literal doc text — flagging this
  explicitly so you can confirm or drop it before `init.sql` is written
  (T5 below).

**Explicitly deferred (documented, not attempted):**
- Porting `Database`/`UserRepository` (raw `sqlite3_*` C API calls) to
  `libpqxx`/`libpq` — different placeholder syntax (`$1` vs `?`), different
  unique-violation error surface, `AUTOINCREMENT` → `SERIAL`/`GENERATED
  ALWAYS AS IDENTITY` schema translation.
- Any data migration from the existing `data/kungfu_chess.db` SQLite file.
- Wiring a real connection string / env var for the Postgres DSN into
  `main.cpp` (there's currently no config plumbing to hang it on).

This is a deliberate honesty boundary: the write-up should say plainly that
Postgres is present but empty, and that the SQLite→Postgres port is real,
non-trivial follow-on work, not a checkbox.

---

## Redis: what's in scope

**In scope now:** a `redis` service in Compose (official image, single
instance), reachable on the network, plus a `redis-cli ping`-based
healthcheck. Nothing reads or writes it — the healthcheck is a pure
liveness signal, not evidence of integration.

**Explicitly deferred:** any actual use — session/room directory keys,
matchmaking rating sorted set, capacity counters, liveness keys — all of it
requires a Redis C++ client dependency and real integration work that isn't
part of this step. The write-up should be explicit that this container does
nothing yet; it exists only to demonstrate the topology's direction.

---

## docker-compose.yml — service plan (described, not yet written as code)

**`kungfu_server`**
- `build:` context = repo root, `dockerfile: server/Dockerfile`
- Multi-stage build: a `builder` stage (Debian/Ubuntu + `build-essential`,
  `cmake`, `ninja-build`, `git`, `ca-certificates` — `git`/`ca-certificates`
  needed because `server/CMakeLists.txt`'s `FetchContent` pulls Asio and
  websocketpp from GitHub at configure time) that builds **only the
  `kungfu_server` CMake target** (not `kungfu_client`/`kungfu_server_tests`,
  to avoid pulling in doctest and extra build time for targets this
  container doesn't need); a slim runtime stage (`debian:bookworm-slim` or
  similar, just a libstdc++ runtime, no build tools) that copies the built
  binary plus `boards/standard.txt` into matching relative paths under
  `/app`.
- `ports:` `"9002:9002"`
- `volumes:` a named volume `kungfu_data:/app/data` so the SQLite file
  survives `docker compose down`/`up` cycles
- `environment:` none required (see simplification above)
- `healthcheck:` a basic TCP-port-open check on 9002, e.g. `nc -z
  localhost 9002`. **Caveat, stated explicitly for the write-up too:** this
  only proves the OS-level listen socket is open — it says nothing about
  whether the websocketpp/Asio event loop is actually still processing
  messages (a hung-but-still-listening process would still report
  healthy). It's also not a zero-cost addition like the other three
  healthchecks here: the `debian:bookworm-slim` runtime image doesn't ship
  `netcat` by default, so the runtime stage in `server/Dockerfile` (T2)
  needs one small added package (e.g. `netcat-openbsd`) just to make this
  check possible.
- No `depends_on` on postgres/redis — there is no real runtime coupling yet,
  and adding one would misrepresent that as existing. The healthchecks
  below are for `docker compose ps`/observability visibility only; none of
  them gate startup order via `depends_on: condition: service_healthy`.

**`postgres`**
- `image: postgres:16-alpine` (or current stable tag)
- `environment:` `POSTGRES_USER`, `POSTGRES_PASSWORD`, `POSTGRES_DB` — via a
  gitignored `.env` file with a committed `.env.example` placeholder (don't
  commit real/default passwords)
- `volumes:` named volume `pgdata:/var/lib/postgresql/data`, plus a
  read-only bind mount of `init.sql` to
  `/docker-entrypoint-initdb.d/init.sql` so the schema (see above) is
  created automatically the first time the volume is empty
- `healthcheck:` `pg_isready` (ships in the official image, zero-cost)
- `ports:` optionally `"5432:5432"` exposed to the host for manual `psql`
  inspection/grading visibility

**`redis`**
- `image: redis:7-alpine`
- `healthcheck:` `redis-cli ping` (ships in the official image, zero-cost)
- `ports:` optionally `"6379:6379"` for manual `redis-cli` inspection
- no volume — nothing writes to it, so persistence is moot

**Shared:** all three on Compose's default bridge network (sufficient since
nothing is wired between them yet; no custom network needed for this step).

---

## What this Compose version proves / does not prove

**Proves:**
- The existing monolithic server, previously only ever built on Windows/
  MSYS2, actually compiles and runs correctly under a Linux toolchain and
  container — a real portability question the codebase has never answered
  before.
- Docker Compose can bring up multiple independent services with a single
  command, with persistent named volumes for stateful ones.
- The Compose topology's shape reflects `Server_Design.md`'s direction
  (separate datastore/cache tier from the compute tier), even though the
  wiring isn't real yet.
- Per-service health signals exist for all three containers (`postgres`
  via `pg_isready`, `redis` via `redis-cli ping`, `kungfu_server` via a
  basic port-open check) — visible in `docker compose ps`/monitoring, even
  though nothing gates startup order on them yet.
- Postgres starts up with a real schema in place (`users`, plus the
  flagged `matches` extrapolation — see the PostgreSQL section above),
  reconciled against `Server_Design.md` Section 1.3 rather than invented
  fresh, even though no app code touches it.

**Does NOT prove:**
- No horizontal scaling: `kungfu_server` is still single-process/
  single-thread; running `--scale kungfu_server=2` would have two instances
  both bound to 9002 with no shared state/Directory — must stay at 1
  replica, and the write-up should say so explicitly rather than let it go
  unstated.
- No actual Postgres or Redis integration — both are idle containers; the
  Postgres schema exists but nothing reads/writes it.
- No Gateway/Auth/Matchmaking/Allocator decomposition — everything is still
  one binary.
- No graceful shutdown — no SIGTERM/SIGINT handler exists in the code, so
  `docker compose down` sends SIGTERM, waits Compose's default ~10s grace
  period, then SIGKILLs the process. Since nothing in `main.cpp` intercepts
  that signal, a write in progress to `data/kungfu_chess.db` at that exact
  moment could be interrupted mid-transaction, leaving an open WAL file or
  risking corruption on next startup. This is a known, stated limitation,
  not a hidden one — a signal-handler-based graceful shutdown (flush/close
  the SQLite handle before exiting) is a documented follow-up (T9).
- The `kungfu_server` healthcheck only proves TCP port 9002 is accepting
  connections — not that the WebSocket/game-tick event loop is actually
  still responsive (see the caveat in the service plan above).
- No TLS (`ws://`, matching current code, not `wss://`).
- Nothing here validates any of `Server_Design.md`'s throughput/capacity
  math — this is a structural/orchestration proof only, at a scale of 1.

---

## Ordered task breakdown

1. **T1 — Linux build spike (risk reduction).** Confirm the `kungfu_server`
   CMake target actually configures and builds under a Linux g++/cmake/
   ninja toolchain before writing the Dockerfile around it — this has never
   been attempted outside MSYS2/Windows and is the single biggest unknown
   in this plan (in particular: does `FetchContent`-populated websocketpp/
   Asio compile clean under Linux g++, and does the Windows-only
   `ws2_32`/`wsock32` link guard correctly no-op). Can be done via a
   throwaway builder-stage Dockerfile run manually before T2 is finalized.

2. **T2 — Write `server/Dockerfile`.** Multi-stage as described above. The
   runtime stage also installs one small extra package, `netcat-openbsd`
   (not present in `debian:bookworm-slim` by default), needed only so the
   `kungfu_server` healthcheck (below) has something to run — call this out
   in the Dockerfile as the one non-zero-cost healthcheck dependency.
   Verify: `docker build -f server/Dockerfile -t kungfu_server .` succeeds
   from repo root.

3. **T3 — Write `.dockerignore`.** Exclude `server/build/`,
   `kungfu-graphics/`, `.git`, `docs/`, existing `data/*.db` and leftover
   `test_ws_*.db`/`.log` files, **plus `.env` and any stray local log files
   (e.g. `server/server.log`, `*.log` generally)** — the `.env` exclusion
   matters specifically once T5 introduces one (Postgres credentials must
   never end up in a build context or image layer).

4. **T4 — Standalone smoke test.** `docker run -p 9002:9002 kungfu_server`,
   then exercise it with the existing `kungfu_client` CLI test tool (built
   locally, not containerized) to confirm register/login/basic-move flow
   works identically to running the native binary.

5. **T5 — Write `init.sql`.** The `users` and (pending your confirmation)
   `matches` schema described in the PostgreSQL section above — DDL only,
   no seed data, so Postgres stays "present but empty" in terms of actual
   rows. Verify by inspection against Section 1.3 before moving to T6: the
   `users` columns should map 1:1 to `UserRepository`'s current SQLite
   schema (translated to Postgres types), and the `matches` table's
   provenance (an extrapolation, not literal doc DDL) should be visible in
   a comment at the top of the file itself, not just in this plan.

6. **T6 — Write `docker-compose.yml`.** The three services as scoped
   above — including each service's healthcheck and the `init.sql` bind
   mount for `postgres` — plus `.env.example` for the Postgres credentials.

7. **T7 — Full Compose verification.** `docker compose up` brings up all
   three containers; `docker compose ps` reports all three `healthy` (not
   just `running`) once their healthchecks pass; `kungfu_server` passes the
   same smoke test as T4; confirm SQLite data persists across a `docker
   compose down && docker compose up` cycle via the named volume; confirm
   `postgres` was seeded by `init.sql` (`docker compose exec postgres psql
   -c '\dt'` shows `users`/`matches`) and `redis` responds to `redis-cli
   ping` — both otherwise idle/empty.

8. **T8 — Write-up section.** Draft the "proves / does not prove" content
   above into whatever document is submitted alongside the Compose file,
   **including the SQLite/SIGTERM paragraph verbatim** (`docker compose
   down` → SIGTERM → ~10s grace → SIGKILL, no signal handler in `main.cpp`
   today, so a write in flight to `data/kungfu_chess.db` at that instant
   could leave an open WAL file or risk corruption — stated as a known
   limitation, with graceful shutdown as a T9 follow-up).

9. **T9 — Documented follow-ups (not implemented this pass).** SQLite→
   Postgres migration (`libpqxx` port of `Database`/`UserRepository` +
   schema translation, and actually wiring the `matches` table if kept),
   real Redis wiring (matchmaking queue or session cache), Gateway/
   WS-Gateway split, **signal-handler-based graceful shutdown** (flush/close
   the SQLite handle on SIGTERM before exit — directly addresses the T8
   corruption risk), env-var config plumbing for port/db-path, and real
   `depends_on: condition: service_healthy` once there's an actual runtime
   coupling to express (the healthchecks added in T6 are observability-only
   until then).

---

## Verification

- `make` + `./run_tests.exe` still pass unmodified (no engine/server source
  changes in this plan) — confirms this work is purely additive.
- `docker build -f server/Dockerfile -t kungfu_server .` succeeds (T2).
- `docker run -p 9002:9002 kungfu_server` + `kungfu_client` smoke test
  passes (T4).
- `docker compose up` brings up all three services; `docker compose ps`
  shows all three `healthy` via their respective healthchecks; SQLite
  persistence survives a down/up cycle; `psql \dt` confirms `init.sql`
  created the expected tables; `redis-cli ping` confirms Redis reachable —
  Postgres/Redis otherwise untouched (T7).
