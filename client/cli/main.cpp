// Task B1: connectivity smoke test only. Establishes a real WebSocket
// connection to kungfu_server (A5) and confirms the handshake plus the
// first state broadcast round-trip. No join/username/gameplay protocol
// yet - that's B2/B3/B4. Unlike A1's throwaway linkage proof, this file
// IS the real deliverable: later tasks extend this same main(), they
// don't replace it.
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <iostream>
#include <string>

using client_t = websocketpp::client<websocketpp::config::asio_client>;

int main(int argc, char** argv) {
    std::string uri = argc > 1 ? argv[1] : "ws://127.0.0.1:9002/";

    client_t client;
    // Same reasoning as WebSocketServer::run(): default access logging
    // dumps every frame header/payload, far more volume than a connectivity
    // smoke test needs.
    client.clear_access_channels(websocketpp::log::alevel::all);
    client.init_asio();

    client.set_open_handler([](websocketpp::connection_hdl) {
        std::cout << "connected to server" << std::endl;
    });
    client.set_message_handler([](websocketpp::connection_hdl, client_t::message_ptr msg) {
        std::cout << "received: " << msg->get_payload() << std::endl;
    });
    client.set_close_handler([](websocketpp::connection_hdl) {
        std::cout << "connection closed" << std::endl;
    });
    client.set_fail_handler([&client](websocketpp::connection_hdl hdl) {
        auto con = client.get_con_from_hdl(hdl);
        std::cout << "connection failed: " << con->get_ec().message() << std::endl;
    });

    websocketpp::lib::error_code ec;
    client_t::connection_ptr con = client.get_connection(uri, ec);
    if (ec) {
        std::cout << "could not create connection to " << uri << ": " << ec.message() << std::endl;
        return 1;
    }

    client.connect(con);
    // Blocks running the asio event loop (handshake + incoming broadcasts)
    // until the connection closes or the process is killed - same run-until-
    // killed shape as WebSocketServer::run(), so a live connection is
    // something you can visibly watch, not something that connects and
    // immediately exits.
    client.run();
    return 0;
}
