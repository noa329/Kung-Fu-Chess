// Echo smoke test - proves the CMake/FetchContent-driven websocketpp +
// Asio build works under g++/MSYS2/Ninja. Real game routing (parsing
// commands, calling into GameEngine, broadcasting state) lands in later
// tasks per docs/tasks/server-phase-plan.md; this is deliberately just
// connectivity plumbing.
//
// ASIO_STANDALONE and _WEBSOCKETPP_CPP11_THREAD_ come from
// CMakeLists.txt's target_compile_definitions, not defined here.
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <iostream>

typedef websocketpp::server<websocketpp::config::asio> server_t;

int main() {
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
