#include "WsTestClient.hpp"
#include <sstream>
#include <stdexcept>

WsTestClient::WsTestClient(uint16_t port, int connectTimeoutMs) {
    client_.clear_access_channels(websocketpp::log::alevel::all);
    client_.clear_error_channels(websocketpp::log::elevel::all);
    client_.init_asio();

    client_.set_open_handler([this](websocketpp::connection_hdl) {
        std::lock_guard<std::mutex> lock(mtx_);
        open_ = true;
        cv_.notify_all();
    });
    client_.set_message_handler([this](websocketpp::connection_hdl, client_t::message_ptr msg) {
        std::lock_guard<std::mutex> lock(mtx_);
        messages_.push_back(msg->get_payload());
        cv_.notify_all();
    });
    client_.set_close_handler([this](websocketpp::connection_hdl) {
        std::lock_guard<std::mutex> lock(mtx_);
        closed_ = true;
        cv_.notify_all();
    });
    client_.set_fail_handler([this](websocketpp::connection_hdl) {
        std::lock_guard<std::mutex> lock(mtx_);
        failed_ = true;
        closed_ = true;
        cv_.notify_all();
    });

    std::ostringstream uri;
    uri << "ws://127.0.0.1:" << port << "/";
    websocketpp::lib::error_code ec;
    client_t::connection_ptr con = client_.get_connection(uri.str(), ec);
    if (ec) {
        throw std::runtime_error("WsTestClient: get_connection failed: " + ec.message());
    }
    hdl_ = con->get_handle();
    client_.connect(con);

    networkThread_ = std::thread([this]() { client_.run(); });

    std::unique_lock<std::mutex> lock(mtx_);
    bool ok = cv_.wait_for(lock, std::chrono::milliseconds(connectTimeoutMs),
                            [this] { return open_ || failed_; });
    if (!ok || !open_) {
        lock.unlock();
        client_.stop();
        if (networkThread_.joinable()) networkThread_.join();
        throw std::runtime_error("WsTestClient: failed to connect within timeout");
    }
}

WsTestClient::~WsTestClient() {
    client_.stop();
    if (networkThread_.joinable()) networkThread_.join();
}

void WsTestClient::send(const std::string& text) {
    websocketpp::lib::error_code ec;
    client_.send(hdl_, text, websocketpp::frame::opcode::text, ec);
    if (ec) {
        throw std::runtime_error("WsTestClient: send failed: " + ec.message());
    }
}

std::optional<std::string> WsTestClient::waitForMessage(int timeoutMs) {
    std::unique_lock<std::mutex> lock(mtx_);
    bool ok = cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                            [this] { return !messages_.empty(); });
    if (!ok || messages_.empty()) return std::nullopt;
    std::string msg = std::move(messages_.front());
    messages_.pop_front();
    return msg;
}

void WsTestClient::clearMessages() {
    std::lock_guard<std::mutex> lock(mtx_);
    messages_.clear();
}

bool WsTestClient::isClosed() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return closed_;
}
