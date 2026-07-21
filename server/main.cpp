// Task A5: the real server entry point. Task A1's echo handler and Task
// A1's standalone GameEngine-linkage proof are both superseded now - this
// wires GameCommandParser (A2) + GameSession (A3) + GameStateSerializer
// (A4) together for real via WebSocketServer (A5), which is a far more
// thorough proof that the engine layers link and run correctly than a
// print-and-discard startup check ever was.
#include "WebSocketServer.hpp"

int main() {
    WebSocketServer server(9002);
    server.run();
}
