// Task B1: connectivity smoke test - establishes a real WebSocket
// connection to kungfu_server (A5) and confirms the handshake round-trip.
// Task B3: prompts for a username, sends the server's join message (B2:
// {"type":"join","username":"..."}), and reacts to the assignment -
// prints "You are White"/"You are Black", and (only relevant to White,
// since Black is only ever assigned once White already joined) a "waiting
// for opponent" state until the "opponent_joined" notice arrives.
// Task B4: the real gameplay loop. Every state-tick broadcast (a JSON
// frame with a "board" field and no "type" - see GameStateSerializer, A4)
// gets printed via BoardPrinter; each stdin line is forwarded verbatim as
// the command string to the server - this client deliberately has no
// command parser of its own, since GameCommandParser (A2) already lives
// server-side and is the source of truth for what's a valid command.
//
// Threading model, and why: websocketpp's client.run() blocks on the
// asio event loop, so it can't share a thread with a blocking
// std::getline(std::cin, ...) loop. This runs client.run() on its own
// thread and keeps the interactive stdin loop on main() - the same
// division of labor as scripts/ws_test_client.py (main thread owns
// input(), a background thread owns the socket), for the same reason:
// that script's own history (see git log - the CRLF/duplicate-input
// bugs) already proved that a background thread printing directly to a
// Windows console while another thread has a line half-typed at a
// blocking read can corrupt the pending input buffer, not just interleave
// output. So the network thread never calls std::cout directly - it only
// enqueues text via enqueueOutput(); the main loop drains and prints that
// queue immediately before each blocking std::getline() call, mirroring
// ws_test_client.py's queue-and-flush-before-input() fix exactly.
//
// Unlike A1's throwaway linkage proof, this file IS the real deliverable:
// each task extends this same main(), none of them replace it.
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include "BoardPrinter.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <mutex>
#include <queue>

using client_t = websocketpp::client<websocketpp::config::asio_client>;

namespace {

std::mutex outputMutex;
std::queue<std::string> outputQueue;

// Called from the network thread - never prints directly, see the
// threading-model comment at the top of this file for why.
void enqueueOutput(const std::string& text) {
    std::lock_guard<std::mutex> lock(outputMutex);
    outputQueue.push(text);
}

// Called from main()'s stdin loop only, right before each blocking
// std::getline() - never from the network thread.
void drainOutput() {
    std::queue<std::string> local;
    {
        std::lock_guard<std::mutex> lock(outputMutex);
        std::swap(local, outputQueue);
    }
    while (!local.empty()) {
        std::cout << local.front();
        local.pop();
    }
    std::cout.flush();
}

std::string trimmed(const std::string& raw) {
    size_t start = raw.find_first_not_of(" \t\r\n");
    size_t end = raw.find_last_not_of(" \t\r\n");
    return start == std::string::npos ? "" : raw.substr(start, end - start + 1);
}

std::string promptUsername() {
    std::string username;
    while (true) {
        std::cout << "Enter your username: ";
        if (!std::getline(std::cin, username)) {
            return ""; // stdin closed - caller decides what to do
        }
        username = trimmed(username);
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
        std::ostringstream out;
        out << "You are " << (color == "white" ? "White" : "Black") << ".\n";
        out << (color == "white" ? "Waiting for opponent...\n"
                                  : "Opponent already connected. Game starting!\n");
        enqueueOutput(out.str());
    } else if (type == "opponent_joined") {
        enqueueOutput("Opponent connected: " + j.value("username", "?") + ". Game starting!\n");
    } else if (type == "join_rejected") {
        enqueueOutput("Join rejected: " + j.value("error", "unknown error") + "\n");
    } else if (j.contains("board")) {
        // Periodic state-tick broadcast (GameStateSerializer, A4) - no
        // "type" field at all. BoardPrinter (text_io) prints the exact
        // same token format the text-protocol binary/tests already use.
        auto board = j.at("board").get<std::vector<std::vector<std::string>>>();
        std::ostringstream out;
        BoardPrinter::print(board, out);
        enqueueOutput(out.str());
    }
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
        enqueueOutput("connected to server\n");
        nlohmann::json join{{"type", "join"}, {"username", username}};
        try {
            client.send(hdl, join.dump(), websocketpp::frame::opcode::text);
        } catch (const websocketpp::exception& e) {
            enqueueOutput(std::string("failed to send join: ") + e.what() + "\n");
        }
    });
    client.set_message_handler([](websocketpp::connection_hdl, client_t::message_ptr msg) {
        handleServerMessage(msg->get_payload());
    });
    client.set_close_handler([](websocketpp::connection_hdl) {
        enqueueOutput("connection closed\n");
    });
    client.set_fail_handler([&client](websocketpp::connection_hdl hdl) {
        auto con = client.get_con_from_hdl(hdl);
        enqueueOutput("connection failed: " + con->get_ec().message() + "\n");
    });

    websocketpp::lib::error_code ec;
    client_t::connection_ptr con = client.get_connection(uri, ec);
    if (ec) {
        std::cout << "could not create connection to " << uri << ": " << ec.message() << std::endl;
        return 1;
    }
    websocketpp::connection_hdl hdl = con->get_handle();

    client.connect(con);

    // Network thread: runs the asio event loop (handshake, join response,
    // every state-tick broadcast) for as long as the connection is alive.
    std::thread networkThread([&client]() { client.run(); });

    // Main thread: the interactive gameplay loop. Each line typed is
    // forwarded verbatim as the command string - no client-side parser,
    // GameCommandParser (A2) server-side is the only source of truth for
    // what's valid. An empty line just flushes queued output, matching
    // ws_test_client.py's existing convention.
    std::string line;
    while (true) {
        drainOutput();
        if (!std::getline(std::cin, line)) break;
        line = trimmed(line);
        if (line.empty()) continue;
        try {
            client.send(hdl, line, websocketpp::frame::opcode::text);
        } catch (const websocketpp::exception& e) {
            enqueueOutput(std::string("failed to send command: ") + e.what() + "\n");
        }
    }
    drainOutput();

    client.stop();
    if (networkThread.joinable()) networkThread.join();
    return 0;
}
