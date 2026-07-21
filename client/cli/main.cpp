// Task B1: connectivity smoke test - establishes a real WebSocket
// connection to kungfu_server (A5) and confirms the handshake round-trip.
// Task B3: prompts for a username, sends the server's join message (B2:
// {"type":"join","username":"..."}), and reacts to the assignment -
// prints "You are White"/"You are Black", and (only relevant to White,
// since Black is only ever assigned once White already joined) a "waiting
// for opponent" state until the "opponent_joined" notice arrives. No
// board/move commands yet - that's B4's job; state-tick broadcasts (JSON
// frames with no "type" field) are silently ignored here for now. Unlike
// A1's throwaway linkage proof, this file IS the real deliverable: later
// tasks extend this same main(), they don't replace it.
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>

using client_t = websocketpp::client<websocketpp::config::asio_client>;

namespace {

std::string promptUsername() {
    std::string username;
    while (true) {
        std::cout << "Enter your username: ";
        if (!std::getline(std::cin, username)) {
            return ""; // stdin closed - caller decides what to do
        }
        // Trim surrounding whitespace - cheap insurance against a stray
        // trailing \r, same class of bug the server's ws_test_client.py
        // history already ran into on the command-line side.
        size_t start = username.find_first_not_of(" \t\r\n");
        size_t end = username.find_last_not_of(" \t\r\n");
        username = (start == std::string::npos) ? "" : username.substr(start, end - start + 1);
        if (!username.empty()) return username;
        std::cout << "Username can't be empty." << std::endl;
    }
}

void handleServerMessage(const std::string& payload) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(payload);
    } catch (const nlohmann::json::parse_error&) {
        return; // server only ever sends JSON - shouldn't happen, ignore defensively
    }
    if (!j.is_object()) return;

    std::string type = j.value("type", "");
    if (type == "joined") {
        std::string color = j.value("color", "");
        std::cout << "You are " << (color == "white" ? "White" : "Black") << "." << std::endl;
        if (color == "white") {
            std::cout << "Waiting for opponent..." << std::endl;
        } else {
            std::cout << "Opponent already connected. Game starting!" << std::endl;
        }
    } else if (type == "opponent_joined") {
        std::cout << "Opponent connected: " << j.value("username", "?") << ". Game starting!" << std::endl;
    } else if (type == "join_rejected") {
        std::cout << "Join rejected: " << j.value("error", "unknown error") << std::endl;
    }
    // Anything else (a periodic state-tick broadcast, which has no "type"
    // field at all - see GameStateSerializer) is silently ignored here;
    // printing the board from it via BoardPrinter is Task B4's job.
}

} // namespace

int main(int argc, char** argv) {
    std::string uri = argc > 1 ? argv[1] : "ws://127.0.0.1:9002/";
    std::string username = promptUsername();
    if (username.empty()) {
        std::cout << "no username entered, exiting." << std::endl;
        return 1;
    }

    client_t client;
    // Same reasoning as WebSocketServer::run(): default access logging
    // dumps every frame header/payload, far more volume than this needs.
    client.clear_access_channels(websocketpp::log::alevel::all);
    client.init_asio();

    client.set_open_handler([&client, &username](websocketpp::connection_hdl hdl) {
        std::cout << "connected to server" << std::endl;
        nlohmann::json join{{"type", "join"}, {"username", username}};
        try {
            client.send(hdl, join.dump(), websocketpp::frame::opcode::text);
        } catch (const websocketpp::exception& e) {
            std::cout << "failed to send join: " << e.what() << std::endl;
        }
    });
    client.set_message_handler([](websocketpp::connection_hdl, client_t::message_ptr msg) {
        handleServerMessage(msg->get_payload());
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
