-- T5 (docs/tasks/docker-compose-plan.md): schema seeded once into the
-- postgres service via /docker-entrypoint-initdb.d/ - DDL only, no seed
-- rows, so Postgres stays "present but empty" (nothing in kungfu_server
-- reads or writes this yet, see the plan's PostgreSQL section).
--
-- users: a direct reconciliation, not a new design. Server_Design.md
-- Section 1.3 describes the row informally as "essentially (user_id,
-- username, password_hash, rating, ...)"; the columns below fill in the
-- "..." with what UserRepository's current SQLite schema already
-- concretely uses (src/persistence/UserRepository.cpp, ensureSchema()),
-- translated to Postgres types/idioms (BIGSERIAL instead of SQLite's
-- INTEGER PRIMARY KEY AUTOINCREMENT, TIMESTAMPTZ instead of a unix-ms
-- INTEGER, since nothing reads this column yet and TIMESTAMPTZ is the
-- idiomatic Postgres choice).
--
-- matches: FLAGGED AS AN EXTRAPOLATION, NOT LITERAL DOC DDL.
-- Server_Design.md never gives DDL for a match-history table anywhere -
-- the only DDL in the whole document is for matchmaking_queue (Section
-- 2.5), which is a transient queue table for a different, explicitly
-- alternative design (the doc's actual matchmaking recommendation is the
-- Redis sorted set in Section 1.3 point 3, not a Postgres table), so it
-- is deliberately NOT reproduced here. The columns below instead derive
-- from what Section 1.3 point 4 describes operationally - a completed
-- match "durably records both intended rating deltas" for its two
-- players - rather than being transcribed from the document.

CREATE TABLE users (
    id            BIGSERIAL PRIMARY KEY,
    username      TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    password_salt TEXT NOT NULL,
    rating        INTEGER NOT NULL DEFAULT 1200,
    created_at    TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE matches (
    id                 BIGSERIAL PRIMARY KEY,
    player_white_id    BIGINT NOT NULL REFERENCES users(id),
    player_black_id    BIGINT NOT NULL REFERENCES users(id),
    winner_id          BIGINT REFERENCES users(id), -- NULL = draw
    rating_delta_white INTEGER NOT NULL,
    rating_delta_black INTEGER NOT NULL,
    completed_at       TIMESTAMPTZ NOT NULL DEFAULT now()
);
