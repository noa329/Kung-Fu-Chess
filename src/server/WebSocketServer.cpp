#include "WebSocketServer.hpp"
#include "GameCommandParser.hpp"
#include "GameStateSerializer.hpp"
#include "BoardParser.hpp"
#include "MatchmakingTimeout.hpp"
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
      sessionManager_(kMaxConnectionsPerSession) {
    // logFile_.is_open() can be false (e.g. no write permission in the
    // working directory) - Logger's sink list just omits it rather than
    // failing startup over a log file, console logging still works.
    userRepository_.ensureSchema(); // idempotent - safe every startup
    // Task D3: no initial session any more - a session is only ever
    // created by handleMatch(), once two seekers are actually matched.
}

int WebSocketServer::createSession() {
    int id = sessionManager_.createSession();
    auto session = std::make_unique<GameSession>();
    session->attachLogger(logger_);
    session->engine().startGame(loadStartingPosition());
    sessions_.push_back(std::move(session));
    return id;
}

void WebSocketServer::onOpen(websocketpp::connection_hdl) {
    // Task D3: no session assignment here any more - a connection isn't
    // placed into any GameSession until it logs in and is matched (see
    // handleLogin()/handlePlay()/handleMatch() below).
    std::cout << "connection accepted" << std::endl;
}

void WebSocketServer::sendJson(websocketpp::connection_hdl hdl, const std::string& json) {
    // A connection can die between the moment we decide to send and the
    // actual send() call - an uncaught websocketpp::exception here would
    // take down the whole process over one dead handle, same class of bug
    // as the A5 crash.
    try {
        server_.send(hdl, json, websocketpp::frame::opcode::text);
    } catch (const websocketpp::exception&) {
        // Swallow - see comment above.
    }
}

void WebSocketServer::handleLogin(websocketpp::connection_hdl hdl, const std::string& username,
                                   const std::string& password) {
    AuthResult result = authService_.authenticate(username, password);
    if (!result.ok) {
        sendJson(hdl, nlohmann::json{{"type", "join_rejected"}, {"error", result.error}}.dump());
        return;
    }
    authenticatedUsers_[hdl] = {username, result.user.rating};
    sendJson(hdl, nlohmann::json{{"type", "logged_in"}, {"username", username}}.dump());
}

void WebSocketServer::handlePlay(websocketpp::connection_hdl hdl) {
    auto authIt = authenticatedUsers_.find(hdl);
    if (authIt == authenticatedUsers_.end()) {
        sendJson(hdl, nlohmann::json{{"type", "play_rejected"}, {"error", "ERROR NOT_LOGGED_IN"}}.dump());
        return;
    }
    if (hdlToSeekerId_.count(hdl)) return; // already queued - no-op, not an error

    int seekerId = nextSeekerId_++;
    hdlToSeekerId_[hdl] = seekerId;

    QueuedSeeker seeker;
    seeker.hdl = hdl;
    seeker.enqueuedAt = std::chrono::steady_clock::now();
    seeker.timer = std::make_unique<asio::steady_timer>(server_.get_io_service());
    seeker.timer->expires_after(std::chrono::milliseconds(MatchmakingTimeout::kTimeoutMs));
    seeker.timer->async_wait([this, seekerId](const asio::error_code& ec) {
        if (ec) return; // cancelled - matched before timing out (see handleMatch())
        auto it = queuedSeekers_.find(seekerId);
        if (it == queuedSeekers_.end()) return;
        int elapsedMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - it->second.enqueuedAt).count());
        bool stillQueued = matchmakingQueue_.contains(seekerId);
        if (MatchmakingTimeout::shouldTimeOut(elapsedMs, stillQueued)) {
            handleMatchmakingTimeout(seekerId);
        }
    });
    queuedSeekers_.emplace(seekerId, std::move(seeker));
    matchmakingQueue_.enqueue(seekerId, authIt->second.rating);

    // Single-threaded io_context, and every enqueue checks for a match
    // immediately - so the queue can never accumulate a latent matchable
    // pair left over from an earlier enqueue that didn't check. One
    // tryMatch() call per enqueue is therefore sufficient, not just a
    // convenient simplification.
    auto match = matchmakingQueue_.tryMatch();
    if (match) {
        handleMatch(match->first, match->second);
    } else {
        sendJson(hdl, nlohmann::json{{"type", "searching"}}.dump());
    }
}

void WebSocketServer::handleMatch(int seekerIdA, int seekerIdB) {
    auto itA = queuedSeekers_.find(seekerIdA);
    auto itB = queuedSeekers_.find(seekerIdB);
    // Both ids came straight out of matchmakingQueue_.tryMatch(), which
    // only ever returns ids enqueue()'d alongside a queuedSeekers_ entry
    // in handlePlay() - these lookups can't actually fail.
    websocketpp::connection_hdl hdlA = itA->second.hdl;
    websocketpp::connection_hdl hdlB = itB->second.hdl;

    itA->second.timer->cancel();
    itB->second.timer->cancel();
    queuedSeekers_.erase(itA);
    queuedSeekers_.erase(itB);
    hdlToSeekerId_.erase(hdlA);
    hdlToSeekerId_.erase(hdlB);

    std::string userA = authenticatedUsers_.at(hdlA).username;
    std::string userB = authenticatedUsers_.at(hdlB).username;

    int sessionId = createSession();
    sessionManager_.tryAdd(sessionId, hdlA);
    sessionManager_.tryAdd(sessionId, hdlB);

    JoinResult resultA = sessions_[sessionId]->assignSeat(userA);
    JoinResult resultB = sessions_[sessionId]->assignSeat(userB);

    sendJson(hdlA, nlohmann::json{
        {"type", "joined"}, {"color", colorName(resultA.color)}, {"username", userA}, {"opponent", userB}
    }.dump());
    sendJson(hdlB, nlohmann::json{
        {"type", "joined"}, {"color", colorName(resultB.color)}, {"username", userB}, {"opponent", userA}
    }.dump());

    broadcastState(sessionId);
}

void WebSocketServer::handleMatchmakingTimeout(int seekerId) {
    auto it = queuedSeekers_.find(seekerId);
    if (it == queuedSeekers_.end()) return;
    websocketpp::connection_hdl hdl = it->second.hdl;

    matchmakingQueue_.remove(seekerId);
    queuedSeekers_.erase(it);
    hdlToSeekerId_.erase(hdl);

    sendJson(hdl, nlohmann::json{{"type", "matchmaking_timeout"}, {"error", "ERROR NO_OPPONENT_FOUND"}}.dump());
}

void WebSocketServer::onMessage(websocketpp::connection_hdl hdl, server_t::message_ptr msg) {
    const std::string& payload = msg->get_payload();

    auto sessionIdOpt = sessionManager_.sessionFor(hdl);
    if (sessionIdOpt) {
        int sessionId = *sessionIdOpt;
        auto parsed = GameCommandParser::parse(payload);
        if (parsed.ok) {
            auto result = sessions_[sessionId]->handleCommand(parsed.command);
            if (!result.ok) {
                std::cout << "rejected command: " << payload << " (" << result.error << ")" << std::endl;
            }
        } else {
            // Byte length + escaped repr, not just the raw payload - a trailing
            // \r (invisible on a terminal, since it just returns the cursor to
            // column 0) reads as "the same 6 characters I typed" while actually
            // being 7 bytes, which is exactly the kind of malformed command this
            // needs to make visible. Routed through logger_ (not std::cout
            // directly) so it lands in server.log too, not just the console.
            logger_.log("malformed command: \"" + escapeForLog(payload) + "\" ("
                         + std::to_string(payload.size()) + " bytes) (" + parsed.error + ")");
        }
        broadcastState(sessionId);
        return;
    }

    // Not yet in a session - the only messages that make sense here are
    // login and play (Task D3's connection lifecycle, see the class
    // comment in WebSocketServer.hpp).
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(payload);
    } catch (const nlohmann::json::parse_error&) {
        return; // garbage before login/matching - there's no game to log it against
    }
    if (!j.is_object()) return;

    std::string type = j.value("type", "");
    if (type == "join") {
        std::string username = j.value("username", "");
        std::string password = j.value("password", "");
        if (username.empty() || password.empty()) {
            sendJson(hdl, nlohmann::json{{"type", "join_rejected"}, {"error", "ERROR MALFORMED_JOIN"}}.dump());
            return;
        }
        handleLogin(hdl, username, password);
    } else if (type == "play") {
        handlePlay(hdl);
    }
    // Anything else before login/matching is silently ignored.
}

void WebSocketServer::broadcastState(int sessionId) {
    std::string state = GameStateSerializer::serialize(sessions_[sessionId]->engine().snapshot());
    for (const auto& hdl : sessionManager_.connectionsIn(sessionId)) {
        // A connection can die between ticks (client process killed,
        // network drop) before we hear about it - send() throws in that
        // case. SessionManager/ConnectionRegistry has no remove() yet
        // (that's Task D4's proper disconnect-handling job, not this one),
        // so a dead handle just keeps failing silently here on every
        // subsequent tick. That's an acceptable gap for A5's scope; an
        // *uncaught* exception taking down the whole server process over
        // one dead connection is not - confirmed by a real crash during
        // manual testing, not a guess.
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
        // Every session ticks and broadcasts independently, all still on
        // this one thread (single-threaded io_context, no worker threads -
        // the confirmed concurrency model, not a new one).
        for (size_t sessionId = 0; sessionId < sessions_.size(); ++sessionId) {
            sessions_[sessionId]->engine().wait(elapsedMs);
            broadcastState(static_cast<int>(sessionId));
        }
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
