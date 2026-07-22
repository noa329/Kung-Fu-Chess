// Task A5: the real server entry point. Task A1's echo handler and Task
// A1's standalone GameEngine-linkage proof are both superseded now - this
// wires GameCommandParser (A2) + GameSession (A3) + GameStateSerializer
// (A4) together for real via WebSocketServer (A5), which is a far more
// thorough proof that the engine layers link and run correctly than a
// print-and-discard startup check ever was.
#include "WebSocketServer.hpp"
#include <filesystem>

int main() {
    // Task C3: data/kungfu_chess.db lives here. SQLite can create the .db
    // file itself on first run but not a missing parent directory - a
    // no-op if data/ already exists.
    std::filesystem::create_directories("data");

    WebSocketServer server(9002);
    server.run();
}
