#ifndef SERVER_TESTS_SERVER_FIXTURE_H
#define SERVER_TESTS_SERVER_FIXTURE_H
#include "WebSocketServer.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

// Task G2: spins up a real WebSocketServer on a background thread for one
// test. Each fixture gets its own port and its own throwaway
// data/test_ws_<port>.db + .log (deleted before use) instead of the real
// dev data/kungfu_chess.db and server.log every manual/production run
// uses - so running this suite repeatedly never accumulates test users in
// the real database or noise in the real log, and two tests can never
// collide on the same port even though doctest runs TEST_CASEs
// sequentially by default (no reliance on that ordering guarantee).
class ServerFixture {
public:
    ServerFixture() : port_(nextPort()) {
        std::filesystem::create_directories("data");
        std::string dbPath = "data/test_ws_" + std::to_string(port_) + ".db";
        std::string logPath = "data/test_ws_" + std::to_string(port_) + ".log";
        std::filesystem::remove(dbPath);
        std::filesystem::remove(logPath);

        server_ = std::make_unique<WebSocketServer>(port_, dbPath, logPath);
        thread_ = std::thread([this] { server_->run(); });

        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!server_->isListening()) {
            if (std::chrono::steady_clock::now() > deadline) {
                server_->stop();
                if (thread_.joinable()) thread_.join();
                throw std::runtime_error("ServerFixture: server did not start listening in time");
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    ~ServerFixture() {
        server_->stop();
        if (thread_.joinable()) thread_.join();
    }

    ServerFixture(const ServerFixture&) = delete;
    ServerFixture& operator=(const ServerFixture&) = delete;

    uint16_t port() const { return port_; }

private:
    static uint16_t nextPort() {
        static std::atomic<uint16_t> counter{19100};
        return counter++;
    }

    uint16_t port_;
    std::unique_ptr<WebSocketServer> server_;
    std::thread thread_;
};
#endif
