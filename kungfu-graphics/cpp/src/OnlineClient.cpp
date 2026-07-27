#include "OnlineClient.hpp"
#include "GameStateDeserializer.hpp"
#include "NetworkEventParser.hpp"
#include "DisconnectStatusDeserializer.hpp"
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include <atomic>
#include <mutex>
#include <thread>

struct OnlineClient::Impl {
    using client_t = websocketpp::client<websocketpp::config::asio_client>;

    client_t client;
    websocketpp::connection_hdl hdl;
    std::thread networkThread;
    std::mutex mtx;
    std::optional<bool> connectionOpened;
    std::optional<OnlineClient::ServerResponse> pendingResponse;
    std::optional<GameSnapshot> latestSnapshot; // Task H5, see pollGameState()
    std::vector<std::variant<SoundEvent, GameLifecycleEvent>> pendingEvents; // Task H6, see pollEvents()
    std::optional<DisconnectStatus> latestDisconnectStatus; // Task H7, see pollDisconnectStatus()
    std::atomic<bool> connected{false};
    bool connectCalled = false;

    // Mirrors client/cli/main.cpp's own handleServerMessage() dispatch
    // exactly, minus the text-output side effects (this class has no UI
    // of its own - OnlineMenuView draws whatever runOnlineGame() tells it
    // to, based on what pollResponse() reports).
    void handleMessage(const std::string& payload) {
        nlohmann::json j;
        try {
            j = nlohmann::json::parse(payload);
        } catch (const nlohmann::json::parse_error&) {
            return;
        }
        if (!j.is_object()) return;

        std::string type = j.value("type", std::string());
        if (type == "logged_in") {
            std::lock_guard<std::mutex> lock(mtx);
            OnlineClient::ServerResponse r;
            r.ok = true;
            pendingResponse = r;
        } else if (type == "join_rejected" || type == "matchmaking_timeout" || type == "join_room_rejected"
                   || type == "create_room_rejected") {
            std::lock_guard<std::mutex> lock(mtx);
            OnlineClient::ServerResponse r;
            r.ok = false;
            r.error = j.value("error", std::string("unknown error"));
            pendingResponse = r;
        } else if (type == "reconnected") {
            std::lock_guard<std::mutex> lock(mtx);
            OnlineClient::ServerResponse r;
            r.ok = true;
            r.reconnected = true;
            r.color = j.value("color", std::string());
            r.opponent = j.value("opponent", std::string("?"));
            pendingResponse = r;
        } else if (type == "searching") {
            // Deliberately no-op, same as client/cli - runOnlineGame()
            // shows its own "Searching..." status synchronously right
            // after sendPlay() instead of waiting on this ack (matchmaking
            // can take up to 60 real seconds).
        } else if (type == "joined") {
            std::lock_guard<std::mutex> lock(mtx);
            OnlineClient::ServerResponse r;
            r.ok = true;
            r.color = j.value("color", std::string());
            r.opponent = j.value("opponent", std::string("?"));
            r.whiteUsername = j.value("whiteUsername", std::string());
            r.blackUsername = j.value("blackUsername", std::string());
            pendingResponse = r;
        } else if (type == "room_created") {
            std::lock_guard<std::mutex> lock(mtx);
            OnlineClient::ServerResponse r;
            r.ok = true;
            r.color = j.value("color", std::string("white"));
            r.roomId = j.value("roomId", std::string("?"));
            pendingResponse = r;
        } else if (type.empty()) {
            // Task H5: the periodic, untyped state-tick broadcast - only
            // ever sent once this connection is inside a session (after a
            // successful match/room-join). GameStateDeserializer does its
            // own JSON parse + "has a board key" check internally, so this
            // just hands it the raw payload rather than re-deriving that
            // check from `j` above. Overwrites the single "latest" slot
            // (see pollGameState()'s own comment) rather than queuing.
            if (auto snap = GameStateDeserializer::deserialize(payload)) {
                std::lock_guard<std::mutex> lock(mtx);
                latestSnapshot = std::move(snap);
                // Task H7: same message, a separate side-channel field set
                // (plan decision 3) - parsed independently rather than
                // folded into GameSnapshot, but coalesced the same way
                // (see pollDisconnectStatus()'s own comment), since it's
                // continuous per-tick state, not a discrete one-shot event
                // like G4's sound/lifecycle pushes above.
                latestDisconnectStatus =
                    DisconnectStatusDeserializer::deserialize(payload).value_or(DisconnectStatus{});
            }
        } else if (type == "sound" || type == "lifecycle") {
            // Task H6: G4's discrete event pushes - queued (not coalesced),
            // since every entry must fire exactly once (see pollEvents()'s
            // own comment). NetworkEventParser does its own JSON parse
            // internally, same "hand it the raw payload" pattern as
            // GameStateDeserializer above.
            if (auto ev = NetworkEventParser::parse(payload)) {
                std::lock_guard<std::mutex> lock(mtx);
                pendingEvents.push_back(std::move(*ev));
            }
        }
    }

    void send(const nlohmann::json& j) {
        try {
            client.send(hdl, j.dump(), websocketpp::frame::opcode::text);
        } catch (const websocketpp::exception&) {
            // Nothing meaningful to do with a send failure on a dead
            // connection here - isConnected() already reports the socket
            // as down via the close/fail handlers, which is what
            // runOnlineGame() actually checks.
        }
    }

    void sendRaw(const std::string& raw) {
        try {
            client.send(hdl, raw, websocketpp::frame::opcode::text);
        } catch (const websocketpp::exception&) {
            // Same reasoning as send() above.
        }
    }
};

OnlineClient::OnlineClient() : impl_(std::make_unique<Impl>()) {
    // Same reasoning as WebSocketServer::run()/client/cli: default access
    // logging dumps every frame header/payload, far more volume than this
    // needs.
    impl_->client.clear_access_channels(websocketpp::log::alevel::all);
    impl_->client.init_asio();

    impl_->client.set_open_handler([this](websocketpp::connection_hdl) {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        impl_->connectionOpened = true;
        impl_->connected = true;
    });
    impl_->client.set_fail_handler([this](websocketpp::connection_hdl hdl) {
        (void)hdl;
        std::lock_guard<std::mutex> lock(impl_->mtx);
        impl_->connectionOpened = false;
        impl_->connected = false;
    });
    impl_->client.set_close_handler([this](websocketpp::connection_hdl) { impl_->connected = false; });
    impl_->client.set_message_handler([this](websocketpp::connection_hdl, Impl::client_t::message_ptr msg) {
        impl_->handleMessage(msg->get_payload());
    });
}

OnlineClient::~OnlineClient() {
    disconnect();
}

void OnlineClient::connect(const std::string& uri) {
    websocketpp::lib::error_code ec;
    Impl::client_t::connection_ptr con = impl_->client.get_connection(uri, ec);
    if (ec) {
        std::lock_guard<std::mutex> lock(impl_->mtx);
        impl_->connectionOpened = false;
        return;
    }
    impl_->hdl = con->get_handle();
    impl_->client.connect(con);
    impl_->connectCalled = true;
    impl_->networkThread = std::thread([this]() { impl_->client.run(); });
}

void OnlineClient::sendLogin(const std::string& username, const std::string& password) {
    impl_->send(nlohmann::json{{"type", "join"}, {"username", username}, {"password", password}});
}

void OnlineClient::sendPlay() {
    impl_->send(nlohmann::json{{"type", "play"}});
}

void OnlineClient::sendCreateRoom() {
    impl_->send(nlohmann::json{{"type", "create_room"}});
}

void OnlineClient::sendJoinRoom(const std::string& roomId) {
    impl_->send(nlohmann::json{{"type", "join_room"}, {"roomId", roomId}});
}

void OnlineClient::sendCommand(const std::string& raw) {
    impl_->sendRaw(raw);
}

void OnlineClient::disconnect() {
    if (!impl_->connectCalled) return;
    if (impl_->connected) {
        websocketpp::lib::error_code ec;
        impl_->client.close(impl_->hdl, websocketpp::close::status::normal, "client disconnect", ec);
    }
    impl_->client.stop();
    if (impl_->networkThread.joinable()) impl_->networkThread.join();
    impl_->connectCalled = false;
}

std::optional<bool> OnlineClient::pollConnectionOpened() {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    if (!impl_->connectionOpened.has_value()) return std::nullopt;
    bool result = *impl_->connectionOpened;
    impl_->connectionOpened.reset();
    return result;
}

std::optional<OnlineClient::ServerResponse> OnlineClient::pollResponse() {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    if (!impl_->pendingResponse.has_value()) return std::nullopt;
    ServerResponse result = *impl_->pendingResponse;
    impl_->pendingResponse.reset();
    return result;
}

std::optional<GameSnapshot> OnlineClient::pollGameState() {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    if (!impl_->latestSnapshot.has_value()) return std::nullopt;
    GameSnapshot result = std::move(*impl_->latestSnapshot);
    impl_->latestSnapshot.reset();
    return result;
}

std::vector<std::variant<SoundEvent, GameLifecycleEvent>> OnlineClient::pollEvents() {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    std::vector<std::variant<SoundEvent, GameLifecycleEvent>> result;
    result.swap(impl_->pendingEvents);
    return result;
}

std::optional<DisconnectStatus> OnlineClient::pollDisconnectStatus() {
    std::lock_guard<std::mutex> lock(impl_->mtx);
    if (!impl_->latestDisconnectStatus.has_value()) return std::nullopt;
    DisconnectStatus result = *impl_->latestDisconnectStatus;
    impl_->latestDisconnectStatus.reset();
    return result;
}

bool OnlineClient::isConnected() const {
    return impl_->connected;
}
