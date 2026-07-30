# Build context is the repo root (see docker-compose.yml / T2 in
# docs/tasks/docker-compose-plan.md), not server/ - server/CMakeLists.txt
# pulls engine sources from ../src, ../include, ../third_party.

FROM debian:bookworm AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# CMake's configure step evaluates every add_executable() in
# server/CMakeLists.txt up front, so client/cli/main.cpp and tests/ must be
# present even though only kungfu_server is actually compiled below -
# confirmed by T1's build spike (configure fails otherwise).
RUN cmake -S server -B server/build -G Ninja -DCMAKE_BUILD_TYPE=Release && \
    cmake --build server/build --target kungfu_server

FROM debian:bookworm-slim AS runtime

# netcat-openbsd is here solely for this image's healthcheck (a basic
# TCP-port-open probe on 9002) - it's not a runtime dependency of
# kungfu_server itself. libstdc++6 is the actual runtime dependency (the
# GNU C++ runtime the builder stage's g++ linked against).
RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Mirrors the CWD-relative paths WebSocketServer/main.cpp already assume
# (data/kungfu_chess.db, boards/standard.txt, server.log) - see "Key
# simplification: no C++ changes are required" in the plan doc. No source
# changes needed, just matching the relative layout under /app.
COPY --from=builder /src/server/build/kungfu_server ./kungfu_server
COPY boards/standard.txt ./boards/standard.txt

EXPOSE 9002

ENTRYPOINT ["./kungfu_server"]
