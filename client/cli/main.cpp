// Task B1: connectivity smoke test - establishes a real WebSocket
// connection to kungfu_server (A5) and confirms the handshake round-trip.
// Task B4: the real gameplay loop. Every state-tick broadcast (a JSON
// frame with a "board" field and no "type" - see GameStateSerializer, A4)
// gets printed via BoardPrinter; each stdin line is forwarded verbatim as
// the command string to the server - this client deliberately has no
// command parser of its own, since GameCommandParser (A2) already lives
// server-side and is the source of truth for what's a valid command.
// Task C4: also prompts for a password and retries the login prompt on
// rejection instead of leaving the user stuck.
// Task D3: login ("join") no longer immediately assigns a color - it just
// authenticates ("logged_in"). This client now follows up with
// {"type":"play"} to enter real matchmaking, printing "Searching for an
// opponent..." and waiting for either "joined" (matched - color and
// opponent both included now, since matching is atomic) or
// "matchmaking_timeout" (no opponent found within the server's 60s
// window - this client just reports it and exits, no auto-retry).
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
// ws_test_client.py's queue-and-flush-before-input() fix exactly. The
// login/matchmaking waits (C4/D3) extend this same rule: waiting for a
// server response uses a separate mutex/condition_variable, not stdout,
// so the network thread still never touches std::cout. The one exception
// is the "Searching for an opponent..." line, printed synchronously by
// the main thread itself right after sending "play" rather than waiting
// for the server's own "searching" ack - the wait for a match can take up
// to 60 real seconds, and nothing drains the output queue while blocked
// on that wait, so relying on the server's ack to show this line would
// leave the user staring at nothing for up to a minute before it appeared.
//
// Unlike A1's throwaway linkage proof, this file IS the real deliverable:
// each task extends this same main(), none of them replace it.
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include "BoardPrinter.hpp"
#include <condition_variable>
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

std::string promptLine(const std::string& prompt) {
    std::string value;
    while (true) {
        std::cout << prompt;
        if (!std::getline(std::cin, value)) {
            return ""; // stdin closed - caller decides what to do
        }
        value = trimmed(value);
        if (!value.empty()) return value;
        std::cout << "Can't be empty." << std::endl;
    }
}

// Task C4/D3: coordinates the main thread's sequential waits (first for
// the login response, then for the matchmaking response) with the
// asynchronous replies arriving on the network thread via
// handleServerMessage(). Reused across both waits since they're strictly
// sequential, never concurrent - not used past a successful match, the
// gameplay loop that follows never touches this again.
struct LoginState {
    std::mutex mtx;
    std::condition_variable cv;
    bool connectionOpen = false;
    bool responsePending = false; // true from the moment a request is sent until a response arrives
    bool ok = false;
    std::string error;
    // Populated only once matched ("joined") - meaningless before then.
    std::string color;
    std::string opponent;
};

void handleServerMessage(const std::string& payload, LoginState& login) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(payload);
    } catch (const nlohmann::json::parse_error&) {
        return; // server only ever sends JSON - shouldn't happen, ignore defensively
    }
    if (!j.is_object()) return;

    std::string type = j.value("type", "");
    if (type == "logged_in") {
        std::lock_guard<std::mutex> lock(login.mtx);
        login.ok = true;
        login.responsePending = false;
        login.cv.notify_all();
    } else if (type == "join_rejected") {
        // No enqueueOutput() here - the login retry loop (main thread)
        // prints the failure and the next prompt together, synchronously,
        // instead of racing an async-queued message against its own
        // immediately-following prompt.
        std::lock_guard<std::mutex> lock(login.mtx);
        login.ok = false;
        login.error = j.value("error", "unknown error");
        login.responsePending = false;
        login.cv.notify_all();
    } else if (type == "searching") {
        // Deliberately no-op - see the threading-model comment at the top
        // of this file for why the main thread prints this line itself
        // instead of waiting on this ack.
    } else if (type == "joined") {
        std::string color = j.value("color", "");
        std::string opponent = j.value("opponent", "?");
        std::ostringstream out;
        out << "You are " << (color == "white" ? "White" : "Black")
            << ". Playing against " << opponent << ".\n";
        enqueueOutput(out.str());

        std::lock_guard<std::mutex> lock(login.mtx);
        login.ok = true;
        login.color = color;
        login.opponent = opponent;
        login.responsePending = false;
        login.cv.notify_all();
    } else if (type == "matchmaking_timeout") {
        std::lock_guard<std::mutex> lock(login.mtx);
        login.ok = false;
        login.error = j.value("error", "unknown error");
        login.responsePending = false;
        login.cv.notify_all();
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

    client_t client;
    // Same reasoning as WebSocketServer::run(): default access logging
    // dumps every frame header/payload, far more volume than this needs.
    client.clear_access_channels(websocketpp::log::alevel::all);
    client.init_asio();

    LoginState login;

    client.set_open_handler([&login](websocketpp::connection_hdl) {
        enqueueOutput("connected to server\n");
        std::lock_guard<std::mutex> lock(login.mtx);
        login.connectionOpen = true;
        login.cv.notify_all();
    });
    client.set_message_handler([&login](websocketpp::connection_hdl, client_t::message_ptr msg) {
        handleServerMessage(msg->get_payload(), login);
    });
    client.set_close_handler([&login](websocketpp::connection_hdl) {
        enqueueOutput("connection closed\n");
        std::lock_guard<std::mutex> lock(login.mtx);
        login.responsePending = false; // unblock a wait stuck on a dead connection
        login.cv.notify_all();
    });
    client.set_fail_handler([&client, &login](websocketpp::connection_hdl hdl) {
        auto con = client.get_con_from_hdl(hdl);
        enqueueOutput("connection failed: " + con->get_ec().message() + "\n");
        std::lock_guard<std::mutex> lock(login.mtx);
        login.responsePending = false;
        login.cv.notify_all();
    });

    websocketpp::lib::error_code ec;
    client_t::connection_ptr con = client.get_connection(uri, ec);
    if (ec) {
        std::cout << "could not create connection to " << uri << ": " << ec.message() << std::endl;
        return 1;
    }
    websocketpp::connection_hdl hdl = con->get_handle();

    client.connect(con);

    // Network thread: runs the asio event loop (handshake, every
    // response, every state-tick broadcast) for as long as the connection
    // is alive.
    std::thread networkThread([&client]() { client.run(); });

    {
        std::unique_lock<std::mutex> lock(login.mtx);
        login.cv.wait(lock, [&login] { return login.connectionOpen; });
    }
    drainOutput(); // shows "connected to server" before the first prompt

    auto shutdown = [&](int status) {
        client.stop();
        if (networkThread.joinable()) networkThread.join();
        return status;
    };

    // Task C4: login retry loop. Prompts for username + password, sends
    // the join (Task D3: login only, no color/session yet - C3's
    // AuthService still auto-registers a never-seen-before username, or
    // verifies an existing one's password), and waits for the server's
    // accept/reject - retrying on reject instead of leaving the user
    // stuck.
    std::string username;
    for (;;) {
        username = promptLine("Enter your username: ");
        if (username.empty()) {
            std::cout << "no username entered, exiting." << std::endl;
            return shutdown(1);
        }
        std::string password = promptLine("Enter your password: ");
        if (password.empty()) {
            std::cout << "no password entered, exiting." << std::endl;
            return shutdown(1);
        }

        nlohmann::json join{{"type", "join"}, {"username", username}, {"password", password}};
        {
            std::lock_guard<std::mutex> lock(login.mtx);
            login.responsePending = true;
        }
        try {
            client.send(hdl, join.dump(), websocketpp::frame::opcode::text);
        } catch (const websocketpp::exception& e) {
            std::cout << "failed to send join: " << e.what() << std::endl;
            return shutdown(1);
        }

        std::unique_lock<std::mutex> lock(login.mtx);
        login.cv.wait(lock, [&login] { return !login.responsePending; });
        bool accepted = login.ok;
        std::string error = login.error;
        lock.unlock();

        if (accepted) break;
        std::cout << "Login failed (" << error << "). Try again." << std::endl;
    }
    std::cout << "Logged in as " << username << "." << std::endl;

    // Task D3: request a match. See the threading-model comment at the
    // top of this file for why "Searching..." is printed here directly
    // rather than via the server's "searching" ack.
    std::cout << "Searching for an opponent..." << std::endl;
    {
        std::lock_guard<std::mutex> lock(login.mtx);
        login.responsePending = true;
    }
    try {
        client.send(hdl, nlohmann::json{{"type", "play"}}.dump(), websocketpp::frame::opcode::text);
    } catch (const websocketpp::exception& e) {
        std::cout << "failed to send play: " << e.what() << std::endl;
        return shutdown(1);
    }

    {
        std::unique_lock<std::mutex> lock(login.mtx);
        login.cv.wait(lock, [&login] { return !login.responsePending; });
        if (!login.ok) {
            std::string matchError = login.error;
            lock.unlock();
            std::cout << "Matchmaking failed (" << matchError << ")." << std::endl;
            return shutdown(1);
        }
    }

    // Main thread: the interactive gameplay loop. Each line typed is
    // forwarded verbatim as the command string - no client-side parser,
    // GameCommandParser (A2) server-side is the only source of truth for
    // what's valid. An empty line just flushes queued output (including
    // the "You are White/Black..." line queued by handleServerMessage's
    // "joined" branch above), matching ws_test_client.py's existing
    // convention.
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

    return shutdown(0);
}
