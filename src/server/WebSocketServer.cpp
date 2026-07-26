#include "WebSocketServer.hpp"
#include "GameCommandParser.hpp"
#include "GameStateSerializer.hpp"
#include "BoardParser.hpp"
#include "MatchmakingTimeout.hpp"
#include "DisconnectTimeout.hpp"
#include <nlohmann/json.hpp>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {

std::string colorName(char color) {
    return color == 'w' ? "white" : (color == 'b' ? "black" : "spectator");
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

WebSocketServer::WebSocketServer(uint16_t port, const std::string& dbPath, const std::string& logPath)
    : port_(port),
      logFile_(logPath, std::ios::app),
      logger_(logFile_.is_open() ? std::vector<std::ostream*>{&std::cout, &logFile_}
                                  : std::vector<std::ostream*>{&std::cout}),
      database_(dbPath),
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
    // Task E3: loadBoard() only, not startGame() - the board is loaded (and
    // will broadcast) immediately either way, but the "start" lifecycle
    // announcement is now deferred to whichever join() call actually
    // completes the two-player roster (announceStart(), called from
    // handleMatch() and handleJoinRoom() - see the class comment).
    session->engine().loadBoard(loadStartingPosition());
    sessions_.push_back(std::move(session));
    return id;
}

void WebSocketServer::onOpen(websocketpp::connection_hdl) {
    // Task D3: no session assignment here any more - a connection isn't
    // placed into any GameSession until it logs in and is matched (see
    // handleLogin()/handlePlay()/handleMatch() below).
    std::cout << "connection accepted" << std::endl;
}

void WebSocketServer::onClose(websocketpp::connection_hdl hdl) {
    // Case 1: was queued in matchmaking, never matched - just dequeue.
    // D4's reconnect support is specifically for a game already in
    // progress (see the class comment); the search queue already has its
    // own independent 60s give-up timeout (D3), unaffected by this.
    auto seekerIt = hdlToSeekerId_.find(hdl);
    if (seekerIt != hdlToSeekerId_.end()) {
        int seekerId = seekerIt->second;
        matchmakingQueue_.remove(seekerId);
        auto qIt = queuedSeekers_.find(seekerId);
        if (qIt != queuedSeekers_.end()) {
            qIt->second.timer->cancel();
            queuedSeekers_.erase(qIt);
        }
        hdlToSeekerId_.erase(seekerIt);
    }

    // Case 2: was in a session - start the 20s auto-resign countdown.
    // Both lookups have to succeed to know *which seat*: sessionManager_
    // for the session id, authenticatedUsers_ for the username
    // GameSession itself is keyed on (colorOf() needs a username, not an
    // hdl - see GameSession.hpp's class comment for why it doesn't know
    // about connection_hdl at all).
    auto sessionIdOpt = sessionManager_.sessionFor(hdl);
    auto authIt = authenticatedUsers_.find(hdl);
    if (sessionIdOpt && authIt != authenticatedUsers_.end()) {
        int sessionId = *sessionIdOpt;
        GameSession& session = *sessions_[sessionId];
        char color = session.colorOf(authIt->second.username);
        // gameOver guard: a connection closing right after the game
        // already ended (win/resign) is just a client tearing down its
        // socket, not a mid-game disconnect - nothing to count down.
        if (color != '\0' && !session.engine().snapshot().gameOver) {
            session.markDisconnected(color);
            sessionManager_.remove(hdl);
            startDisconnectTimer(sessionId, color);
            broadcastState(sessionId);
            logger_.log("disconnect session=" + std::to_string(sessionId) + " color=" + std::string(1, color)
                         + " username=" + authIt->second.username + " - 20s auto-resign countdown started");
        }
    }

    authenticatedUsers_.erase(hdl);
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
    // Task D4: a re-login as a username that currently owns a
    // disconnected seat in an unfinished game resumes that game instead
    // of starting a fresh logged_in/matchmaking flow.
    if (tryReconnect(hdl, username, result.user.rating)) return;
    authenticatedUsers_[hdl] = {username, result.user.rating};
    sendJson(hdl, nlohmann::json{{"type", "logged_in"}, {"username", username}}.dump());
}

bool WebSocketServer::tryReconnect(websocketpp::connection_hdl hdl, const std::string& username, int rating) {
    auto sessIt = usernameToSessionId_.find(username);
    if (sessIt == usernameToSessionId_.end()) return false;

    int sessionId = sessIt->second;
    GameSession& session = *sessions_[sessionId];
    char color = session.colorOf(username);
    if (color == '\0') return false; // shouldn't happen - defensive, mirrors join()'s own stance
    // Game already decided (win, resign, or a prior auto-resign) - this is
    // a fresh login, not a reconnect into anything still playable.
    if (session.engine().snapshot().gameOver) return false;
    // Seat's own connection never actually dropped (e.g. a stray extra
    // "join" message) - nothing to reconnect.
    if (session.isConnected(color)) return false;

    if (!sessionManager_.tryAdd(sessionId, hdl)) return false;

    authenticatedUsers_[hdl] = {username, rating};
    session.markReconnected(color);

    auto seatIt = disconnectedSeats_.find({sessionId, color});
    if (seatIt != disconnectedSeats_.end()) {
        seatIt->second.timer->cancel();
        disconnectedSeats_.erase(seatIt);
    }

    std::string opponent = session.usernameFor(color == 'w' ? 'b' : 'w');
    sendJson(hdl, nlohmann::json{
        {"type", "reconnected"}, {"color", colorName(color)}, {"username", username}, {"opponent", opponent}
    }.dump());
    broadcastState(sessionId);
    logger_.log("reconnect session=" + std::to_string(sessionId) + " color=" + std::string(1, color)
                 + " username=" + username + " - countdown cancelled");
    return true;
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
    // Task D4: remembered so a later re-login as userA/userB can be
    // recognized as a reconnect into this game (see tryReconnect()).
    usernameToSessionId_[userA] = sessionId;
    usernameToSessionId_[userB] = sessionId;

    JoinResult resultA = sessions_[sessionId]->join(userA);
    JoinResult resultB = sessions_[sessionId]->join(userB);
    // Task E3: both players are known the instant this session exists (the
    // exact "2 known participants" moment room joins only reach later, at
    // the seat-filling join - see handleJoinRoom()), so announce start
    // right here rather than relying on createSession() to do it (it no
    // longer does, since a room session needs that deferred).
    sessions_[sessionId]->engine().announceStart();

    sendJoinedMessage(hdlA, sessionId, userA, resultA.color);
    sendJoinedMessage(hdlB, sessionId, userB, resultB.color);

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

void WebSocketServer::sendJoinedMessage(websocketpp::connection_hdl hdl, int sessionId, const std::string& username, char color) {
    GameSession& session = *sessions_[sessionId];
    std::string whiteUsername = session.usernameFor('w');
    std::string blackUsername = session.usernameFor('b');
    // "opponent" keeps its existing single-name meaning for an actual
    // player (matchmaking's original shape, unchanged); a spectator has no
    // single opponent, so it gets a summary of both instead - whiteUsername/
    // blackUsername are also included directly for a client that wants
    // them without parsing that string.
    std::string opponent = (color == 'w') ? blackUsername
                          : (color == 'b') ? whiteUsername
                          : (whiteUsername + " vs " + blackUsername);
    sendJson(hdl, nlohmann::json{
        {"type", "joined"}, {"color", colorName(color)}, {"username", username},
        {"opponent", opponent}, {"whiteUsername", whiteUsername}, {"blackUsername", blackUsername}
    }.dump());
}

void WebSocketServer::handleCreateRoom(websocketpp::connection_hdl hdl) {
    auto authIt = authenticatedUsers_.find(hdl);
    if (authIt == authenticatedUsers_.end()) {
        sendJson(hdl, nlohmann::json{{"type", "create_room_rejected"}, {"error", "ERROR NOT_LOGGED_IN"}}.dump());
        return;
    }
    std::string username = authIt->second.username;

    int sessionId = createSession();
    sessionManager_.tryAdd(sessionId, hdl);
    JoinResult result = sessions_[sessionId]->join(username); // creator is always the 1st join -> White
    usernameToSessionId_[username] = sessionId;

    std::string roomId = roomRegistry_.createRoom(sessionId);
    sendJson(hdl, nlohmann::json{
        {"type", "room_created"}, {"roomId", roomId}, {"color", colorName(result.color)}, {"username", username}
    }.dump());
    // Broadcasts the freshly loaded starting position immediately, rather
    // than waiting for the next tick, so the creator isn't staring at
    // nothing while alone in the room - same reasoning handleMatch() has
    // for its own immediate broadcastState() call.
    broadcastState(sessionId);
}

void WebSocketServer::handleJoinRoom(websocketpp::connection_hdl hdl, const std::string& roomId) {
    auto authIt = authenticatedUsers_.find(hdl);
    if (authIt == authenticatedUsers_.end()) {
        sendJson(hdl, nlohmann::json{{"type", "join_room_rejected"}, {"error", "ERROR NOT_LOGGED_IN"}}.dump());
        return;
    }
    auto sessionIdOpt = roomRegistry_.sessionForRoom(roomId);
    if (!sessionIdOpt) {
        sendJson(hdl, nlohmann::json{{"type", "join_room_rejected"}, {"error", "ERROR ROOM_NOT_FOUND"}}.dump());
        return;
    }
    int sessionId = *sessionIdOpt;
    if (!sessionManager_.tryAdd(sessionId, hdl)) {
        sendJson(hdl, nlohmann::json{{"type", "join_room_rejected"}, {"error", "ERROR ROOM_FULL"}}.dump());
        return;
    }

    std::string username = authIt->second.username;
    JoinResult result = sessions_[sessionId]->join(username);
    usernameToSessionId_[username] = sessionId;

    // The 2nd join (Black) is the exact moment this room's two-player
    // roster is complete - the deferred counterpart to handleMatch()'s own
    // immediate announceStart() call (see the class comment's Task E3
    // paragraph). A 3rd+ join (spectator) never re-fires this.
    if (result.color == 'b') {
        sessions_[sessionId]->engine().announceStart();
    }

    sendJoinedMessage(hdl, sessionId, username, result.color);
    broadcastState(sessionId);
}

void WebSocketServer::startDisconnectTimer(int sessionId, char color) {
    DisconnectedSeat seat;
    seat.disconnectedAt = std::chrono::steady_clock::now();
    seat.timer = std::make_unique<asio::steady_timer>(server_.get_io_service());
    seat.timer->expires_after(std::chrono::milliseconds(DisconnectTimeout::kTimeoutMs));
    seat.timer->async_wait([this, sessionId, color](const asio::error_code& ec) {
        if (ec) return; // cancelled - reconnected before timing out (see tryReconnect())
        auto it = disconnectedSeats_.find({sessionId, color});
        if (it == disconnectedSeats_.end()) return;
        int elapsedMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - it->second.disconnectedAt).count());
        bool stillDisconnected = !sessions_[sessionId]->isConnected(color);
        if (DisconnectTimeout::shouldAutoResign(elapsedMs, stillDisconnected)) {
            handleDisconnectTimeout(sessionId, color);
        }
    });
    disconnectedSeats_[{sessionId, color}] = std::move(seat);
}

void WebSocketServer::handleDisconnectTimeout(int sessionId, char color) {
    disconnectedSeats_.erase({sessionId, color});
    sessions_[sessionId]->engine().resign(color);
    broadcastState(sessionId);
    logger_.log("auto-resign session=" + std::to_string(sessionId) + " color=" + std::string(1, color)
                 + " - 20s disconnect window elapsed with no reconnect");
}

void WebSocketServer::cancelDisconnectTimers(int sessionId) {
    for (char color : {'w', 'b'}) {
        auto it = disconnectedSeats_.find({sessionId, color});
        if (it != disconnectedSeats_.end()) {
            it->second.timer->cancel();
            disconnectedSeats_.erase(it);
        }
    }
}

void WebSocketServer::handleResign(int sessionId, const std::string& username) {
    CommandResult result = sessions_[sessionId]->handleResign(username);
    if (!result.ok) {
        std::cout << "rejected resign: " << username << " (" << result.error << ")" << std::endl;
        return;
    }
    cancelDisconnectTimers(sessionId);
    logger_.log("resign session=" + std::to_string(sessionId) + " username=" + username);
}

void WebSocketServer::onMessage(websocketpp::connection_hdl hdl, server_t::message_ptr msg) {
    const std::string& payload = msg->get_payload();

    auto sessionIdOpt = sessionManager_.sessionFor(hdl);
    if (sessionIdOpt) {
        int sessionId = *sessionIdOpt;
        // Task E2: identity is resolved once here (not just the command's
        // own claimed color) - a session-tracked hdl always has an
        // authenticatedUsers_ entry (login happens before matching/
        // room-join/reconnect ever adds it to sessionManager_), so the
        // empty-string fallback is defensive only, mirroring
        // tryReconnect()'s own "shouldn't happen" stance.
        auto authIt = authenticatedUsers_.find(hdl);
        std::string username = authIt != authenticatedUsers_.end() ? authIt->second.username : "";

        // Task G3: "resign" is a reserved literal command, intercepted
        // before GameCommandParser::parse() - it's unambiguous against
        // that parser's strict-uppercase move/jump grammar, so there's no
        // risk of it ever being a real (if malformed) move/jump attempt.
        if (payload == "resign") {
            handleResign(sessionId, username);
            broadcastState(sessionId);
            return;
        }

        auto parsed = GameCommandParser::parse(payload);
        if (parsed.ok) {
            auto result = sessions_[sessionId]->handleCommand(username, parsed.command);
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
    // login, play, and create/join room (Task D3/E3's connection
    // lifecycle, see the class comment in WebSocketServer.hpp).
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
    } else if (type == "create_room") {
        handleCreateRoom(hdl);
    } else if (type == "join_room") {
        std::string roomId = j.value("roomId", "");
        if (roomId.empty()) {
            sendJson(hdl, nlohmann::json{{"type", "join_room_rejected"}, {"error", "ERROR MALFORMED_JOIN_ROOM"}}.dump());
            return;
        }
        handleJoinRoom(hdl, roomId);
    }
    // Anything else before login/matching is silently ignored.
}

void WebSocketServer::broadcastState(int sessionId) {
    // Task D4: per-color remaining disconnect ms, recomputed from real
    // elapsed time every call (same steady_clock-based measurement D3's
    // matchmaking timeout uses) rather than trusting a stored nominal
    // value - present only for a seat actually mid-countdown right now.
    GameStateSerializer::DisconnectStatus disconnectStatus;
    auto now = std::chrono::steady_clock::now();
    auto remainingMs = [&](char color) -> std::optional<int> {
        auto it = disconnectedSeats_.find({sessionId, color});
        if (it == disconnectedSeats_.end()) return std::nullopt;
        int elapsedMs = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.disconnectedAt).count());
        int remaining = DisconnectTimeout::kTimeoutMs - elapsedMs;
        return remaining > 0 ? remaining : 0;
    };
    disconnectStatus.whiteRemainingMs = remainingMs('w');
    disconnectStatus.blackRemainingMs = remainingMs('b');

    std::string state = GameStateSerializer::serialize(sessions_[sessionId]->engine().snapshot(), disconnectStatus);
    for (const auto& hdl : sessionManager_.connectionsIn(sessionId)) {
        // Normally onClose() (Task D4) removes a dead hdl from
        // sessionManager_ before the next tick, so this loop shouldn't see
        // one - but websocketpp's close/fail handlers firing is not
        // instantaneous with the underlying socket actually dying, so a
        // send() here can still race a handle that's about to close. Keep
        // the try/catch as defense in depth (a real crash during A5
        // manual testing is what put it here originally) rather than
        // trusting handler ordering.
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
    // Task D4: the only way a socket going away was previously observed
    // was a failed send() on the next broadcast tick (silently swallowed,
    // see broadcastState()) - no code ever ran *at* disconnect time. Both
    // route to the same handler: close is a clean disconnect, fail is a
    // connection that never completed the WS handshake or died before
    // close - onClose() itself is a no-op for any hdl it doesn't
    // recognize (never authenticated, never queued, never in a session),
    // so treating both alike here is safe.
    server_.set_close_handler([this](websocketpp::connection_hdl hdl) { onClose(hdl); });
    server_.set_fail_handler([this](websocketpp::connection_hdl hdl) { onClose(hdl); });

    tickTimer_ = std::make_unique<asio::steady_timer>(server_.get_io_service());
    lastTickTime_ = std::chrono::steady_clock::now();
    scheduleTick();

    server_.listen(port_);
    server_.start_accept();
    listening_ = true;
    std::cout << "listening on " << port_ << std::endl;
    server_.run();
}

void WebSocketServer::stop() {
    server_.get_io_service().stop();
}

bool WebSocketServer::isListening() const {
    return listening_;
}
