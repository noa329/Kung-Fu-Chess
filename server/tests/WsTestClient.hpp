#ifndef SERVER_TESTS_WS_TEST_CLIENT_H
#define SERVER_TESTS_WS_TEST_CLIENT_H
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

// Task G2: minimal in-process WebSocket client for WebSocketServer's new
// socket-level test suite. Same websocketpp::client<config::asio_client>
// setup client/cli/main.cpp already uses for the real shell client (open
// handler + background client.run() thread + a queue fed by the message
// handler) - generalized here into a reusable test helper instead of a
// one-off interactive client. Connects over real loopback sockets on
// purpose (not mocked) so tests actually exercise WebSocketServer's own
// onOpen/onMessage/onClose/broadcastState(), not a stand-in for them.
class WsTestClient {
public:
    using client_t = websocketpp::client<websocketpp::config::asio_client>;

    // Connects to ws://127.0.0.1:<port>/. Blocks until the handshake
    // completes or connectTimeoutMs elapses, throwing std::runtime_error
    // on failure/timeout - a fixture bug (e.g. the server not actually
    // listening yet) should fail the test loudly, not hang or silently
    // proceed against a dead connection.
    explicit WsTestClient(uint16_t port, int connectTimeoutMs = 2000);
    ~WsTestClient();

    WsTestClient(const WsTestClient&) = delete;
    WsTestClient& operator=(const WsTestClient&) = delete;

    void send(const std::string& text);

    // Pops the oldest not-yet-consumed message, waiting up to timeoutMs
    // for one to arrive if the queue is currently empty. std::nullopt on
    // timeout - callers decide whether that's a test failure or an
    // expected "nothing else arrives" assertion.
    std::optional<std::string> waitForMessage(int timeoutMs = 2000);

    // Discards any messages already queued (e.g. the first state-tick
    // broadcasts right after joining) so a later waitForMessage() call
    // observes only what happens next, not backlog.
    void clearMessages();

    // True once the close/fail handler has fired.
    bool isClosed() const;

private:
    client_t client_;
    websocketpp::connection_hdl hdl_;
    std::thread networkThread_;

    mutable std::mutex mtx_;
    std::condition_variable cv_;
    bool open_ = false;
    bool failed_ = false;
    bool closed_ = false;
    std::deque<std::string> messages_;
};
#endif
