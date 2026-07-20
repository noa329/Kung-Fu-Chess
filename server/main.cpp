// Echo smoke test - proves the CMake/FetchContent-driven websocketpp +
// Asio build works under g++/MSYS2/Ninja, AND (Task A1) that the reused
// engine layers (model/movement_rules/rule_engine/real_time_arbiter/
// game_engine/controller) link and run correctly into this binary. Real
// game routing (parsing commands, calling GameEngine::select()/jump()
// per message, broadcasting state) lands in Tasks A2-A5 per
// docs/tasks/server-phase-plan.md; the WS handler below still just
// echoes - GameEngine is constructed and exercised once at startup only,
// as a linkage/sanity proof, not wired into message handling yet.
//
// ASIO_STANDALONE and _WEBSOCKETPP_CPP11_THREAD_ come from
// CMakeLists.txt's target_compile_definitions, not defined here.
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include "GameEngine.hpp"
#include <iostream>

typedef websocketpp::server<websocketpp::config::asio> server_t;

namespace {

void proveEngineLinkage() {
    GameEngine engine;
    engine.startGame({{"wK", ".", "."}, {".", ".", "bK"}});
    engine.select({0, 0});
    engine.select({0, 1});
    engine.wait(1000);
    GameSnapshot snap = engine.snapshot();
    std::cout << "engine linkage OK - board after one move: ";
    for (const auto& row : snap.boardTokens) {
        for (const auto& cell : row) std::cout << cell << " ";
    }
    std::cout << std::endl;
}

} // namespace

int main() {
    proveEngineLinkage();

    server_t s;
    s.set_message_handler([&s](websocketpp::connection_hdl hdl, server_t::message_ptr msg) {
        std::cout << "received: " << msg->get_payload() << std::endl;
        s.send(hdl, "echo: " + msg->get_payload(), websocketpp::frame::opcode::text);
    });
    s.init_asio();
    s.set_reuse_addr(true);
    s.listen(9002);
    s.start_accept();
    std::cout << "listening on 9002" << std::endl;
    s.run();
}
