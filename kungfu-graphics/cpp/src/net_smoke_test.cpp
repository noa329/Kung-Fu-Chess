// docs/tasks/graphics-networked-client-plan.md, Task H1: throwaway
// connectivity proof, not the real networked graphics client (that's
// Task H2 on). The only thing this file exists to prove is that
// websocketpp + Asio + nlohmann::json actually compile and link under
// this project's MSVC toolchain - server/ only ever proved this under
// MinGW/g++ (see server/CMakeLists.txt), which is a genuinely different,
// previously-unverified risk for kungfu-graphics/cpp (pinned to MSVC
// because of OpenCV_451's prebuilt MSVC-ABI .lib).
//
// Deliberately single-threaded and self-terminating (unlike client/cli's
// permanent background-thread-plus-interactive-loop design, which Task H5
// on will actually reuse) - this only needs to prove one round trip: open
// a real WebSocket connection to a running kungfu_server.exe, confirm the
// handshake completed, then close cleanly.
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>

using client_t = websocketpp::client<websocketpp::config::asio_client>;

int main(int argc, char** argv) {
    std::string uri = argc > 1 ? argv[1] : "ws://127.0.0.1:9002/";

    // Also proves nlohmann::json compiles/links under MSVC here, not just
    // websocketpp/Asio - Task H2's wire-format translator needs it too.
    nlohmann::json probe{{"type", "net_smoke_test"}, {"uri", uri}};
    std::cout << "attempting: " << probe.dump() << std::endl;

    client_t client;
    client.clear_access_channels(websocketpp::log::alevel::all);
    client.init_asio();

    bool connected = false;

    client.set_open_handler([&client, &connected](websocketpp::connection_hdl hdl) {
        connected = true;
        std::cout << "connected" << std::endl;
        // One round trip is the whole point of this smoke test - close
        // right away so client.run() below returns on its own instead of
        // needing a Ctrl-C.
        client.close(hdl, websocketpp::close::status::normal, "smoke test done");
    });
    client.set_fail_handler([&client](websocketpp::connection_hdl hdl) {
        auto con = client.get_con_from_hdl(hdl);
        std::cout << "connection failed: " << con->get_ec().message() << std::endl;
    });
    client.set_close_handler([](websocketpp::connection_hdl) {
        std::cout << "connection closed" << std::endl;
    });

    websocketpp::lib::error_code ec;
    client_t::connection_ptr con = client.get_connection(uri, ec);
    if (ec) {
        std::cout << "could not create connection to " << uri << ": " << ec.message() << std::endl;
        return 1;
    }
    client.connect(con);
    client.run(); // returns once the connection above closes or fails

    return connected ? 0 : 1;
}
