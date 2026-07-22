#include "WebSocketServer.hpp"
#include "GameCommandParser.hpp"
#include "GameStateSerializer.hpp"
#include "BoardParser.hpp"
#include <nlohmann/json.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {

std::string colorName(char color) {
    return color == 'w' ? "white" : "black";
}

// Renders non-printable bytes (notably a stray \r from a client whose line
// reader didn't strip Windows CRLF endings) visibly instead of letting them
// mangle the log line - a malformed command whose actual byte length
// doesn't match what it looks like on screen is exactly the case this
// exists to catch.
std::string escapeForLog(const std::string& raw) {
    std::ostringstream out;
    for (unsigned char c : raw) {
        switch (c) {
            case '\r': out << "\\r"; break;
            case '\n': out << "\\n"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 0x20 || c == 0x7F) {
                    out << "\\x" << std::hex << std::uppercase << std::setw(2)
                        << std::setfill('0') << static_cast<int>(c) << std::dec;
                } else {
                    out << static_cast<char>(c);
                }
        }
    }
    return out.str();
}

// Cwd-relative, same convention as data/kungfu_chess.db and server.log
// below - the server is expected to run from the repo root. Loaded via
// BoardParser (text_io) instead of a hardcoded literal so this doesn't
// duplicate kungfu-graphics/cpp/src/main.cpp's starting position.
const std::string kBoardPath = "boards/standard.txt";

std::vector<std::vector<std::string>> loadStartingPosition() {
    std::ifstream boardFile(kBoardPath);
    if (!boardFile.is_open()) {
        throw std::runtime_error("Failed to open starting position file \"" + kBoardPath +
                                  "\" (run the server from the repo root).");
    }
    BoardParseResult result = BoardParser::parse(boardFile);
    if (!result.ok || result.tokens.empty()) {
        throw std::runtime_error("Failed to parse starting position from \"" + kBoardPath + "\": " +
                                  (result.ok ? "file has no board rows" : result.error));
    }
    return result.tokens;
}
} // namespace

WebSocketServer::WebSocketServer(uint16_t port)
    : port_(port),
      logFile_("server.log", std::ios::app),
      logger_(logFile_.is_open() ? std::vector<std::ostream*>{&std::cout, &logFile_}
                                  : std::vector<std::ostream*>{&std::cout}),
      database_("data/kungfu_chess.db"),
      userRepository_(database_),
      authService_(userRepository_),
      registry_(kMaxConnections) {
    // logFile_.is_open() can be false (e.g. no write permission in the
    // working directory) - Logger's sink list just omits it rather than
    // failing startup over a log file, console logging still works.
    session_.attachLogger(logger_);
    userRepository_.ensureSchema(); // idempotent - safe every startup
    session_.attachAuthService(authService_);
    session_.engine().startGame(loadStartingPosition());
}

void WebSocketServer::onOpen(websocketpp::connection_hdl hdl) {
    if (!registry_.tryAdd(hdl)) {
        std::cout << "rejecting connection - session full" << std::endl;
        server_.close(hdl, websocketpp::close::status::try_again_later, "session full");
        return;
    }
    std::cout << "connection accepted (" << registry_.activeCount() << "/" << kMaxConnections << ")" << std::endl;
    broadcastState();
}

void WebSocketServer::sendJson(websocketpp::connection_hdl hdl, const std::string& json) {
    // Same reasoning as broadcastState(): a connection can die between the
    // moment we decide to send and the actual send() call - an uncaught
    // websocketpp::exception here would take down the whole process over
    // one dead handle, same class of bug as the A5 crash.
    try {
        server_.send(hdl, json, websocketpp::frame::opcode::text);
    } catch (const websocketpp::exception&) {
        // Swallow - see comment above.
    }
}

bool WebSocketServer::tryHandleJoin(websocketpp::connection_hdl hdl, const std::string& payload) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(payload);
    } catch (const nlohmann::json::parse_error&) {
        return false; // not JSON at all - treat as a plain game command
    }
    if (!j.is_object() || j.value("type", "") != "join") return false;

    std::string username = j.value("username", "");
    std::string password = j.value("password", "");
    if (username.empty() || password.empty()) {
        sendJson(hdl, nlohmann::json{{"type", "join_rejected"}, {"error", "ERROR MALFORMED_JOIN"}}.dump());
        return true;
    }

    JoinResult result = session_.handleJoin(username, password);
    if (!result.ok) {
        sendJson(hdl, nlohmann::json{{"type", "join_rejected"}, {"error", result.error}}.dump());
        return true;
    }

    hdlToColor_[hdl] = result.color;
    sendJson(hdl, nlohmann::json{
        {"type", "joined"}, {"color", colorName(result.color)}, {"username", username}
    }.dump());

    if (result.hasOpponent) {
        for (const auto& [otherHdl, otherColor] : hdlToColor_) {
            if (otherColor != result.color) {
                sendJson(otherHdl, nlohmann::json{{"type", "opponent_joined"}, {"username", username}}.dump());
                break;
            }
        }
    }
    return true;
}

void WebSocketServer::onMessage(websocketpp::connection_hdl hdl, server_t::message_ptr msg) {
    const std::string& payload = msg->get_payload();
    if (tryHandleJoin(hdl, payload)) return;

    auto parsed = GameCommandParser::parse(payload);
    if (parsed.ok) {
        auto result = session_.handleCommand(parsed.command);
        if (!result.ok) {
            std::cout << "rejected command: " << payload << " (" << result.error << ")" << std::endl;
        }
    } else {
        // Byte length + escaped repr, not just the raw payload - a trailing
        // \r (invisible on a terminal, since it just returns the cursor to
        // column 0) reads as "the same 6 characters I typed" while actually
        // being 7 bytes, which is exactly the kind of malformed command this
        // needs to make visible. Routed through logger_ (not std::cout
        // directly) so it lands in server.log too, not just the console -
        // cheap insurance for diagnosing any future report of a truncated/
        // malformed command arriving server-side, without needing a repro
        // captured live.
        logger_.log("malformed command: \"" + escapeForLog(payload) + "\" ("
                     + std::to_string(payload.size()) + " bytes) (" + parsed.error + ")");
    }
    broadcastState();
}

void WebSocketServer::broadcastState() {
    std::string state = GameStateSerializer::serialize(session_.engine().snapshot());
    for (const auto& hdl : registry_.connections()) {
        // A connection can die between ticks (client process killed,
        // network drop) before we hear about it - send() throws in that
        // case. ConnectionRegistry has no remove() yet (that's Task D4's
        // proper disconnect-handling job, not this one), so a dead handle
        // just keeps failing silently here on every subsequent tick. That's
        // an acceptable gap for A5's scope; an *uncaught* exception taking
        // down the whole server process over one dead connection is not -
        // confirmed by a real crash during manual testing, not a guess.
        try {
            server_.send(hdl, state, websocketpp::frame::opcode::text);
        } catch (const websocketpp::exception&) {
            // Swallow - see comment above.
        }
    }
}

void WebSocketServer::scheduleTick() {
    tickTimer_->expires_after(std::chrono::milliseconds(kTickMs));
    tickTimer_->async_wait([this](const asio::error_code& ec) {
        if (ec) return; // timer cancelled (server shutting down)
        auto now = std::chrono::steady_clock::now();
        int elapsedMs = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTickTime_).count());
        lastTickTime_ = now;
        session_.engine().wait(elapsedMs);
        broadcastState();
        scheduleTick();
    });
}

void WebSocketServer::run() {
    // websocketpp's default access logging includes frame_header/
    // frame_payload - a trace-level dump of every single frame sent or
    // received. At a per-tick broadcast to every connection, that's a lot
    // of log volume for no operational benefit. Keep only what's actually
    // useful to an operator watching this run.
    server_.clear_access_channels(websocketpp::log::alevel::all);
    server_.set_access_channels(websocketpp::log::alevel::connect
                                 | websocketpp::log::alevel::disconnect
                                 | websocketpp::log::alevel::fail);

    server_.init_asio();
    server_.set_reuse_addr(true);
    server_.set_open_handler([this](websocketpp::connection_hdl hdl) { onOpen(hdl); });
    server_.set_message_handler([this](websocketpp::connection_hdl hdl, server_t::message_ptr msg) {
        onMessage(hdl, msg);
    });

    tickTimer_ = std::make_unique<asio::steady_timer>(server_.get_io_service());
    lastTickTime_ = std::chrono::steady_clock::now();
    scheduleTick();

    server_.listen(port_);
    server_.start_accept();
    std::cout << "listening on " << port_ << std::endl;
    server_.run();
}
