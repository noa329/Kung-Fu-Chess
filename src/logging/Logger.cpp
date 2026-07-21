#include "Logger.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace {

std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
#ifdef _WIN32
    localtime_s(&tmBuf, &t);
#else
    localtime_r(&t, &tmBuf);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmBuf, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace

Logger::Logger(std::vector<std::ostream*> sinks) : sinks_(std::move(sinks)) {}

void Logger::log(const std::string& message) {
    std::string line = "[" + timestamp() + "] " + message;
    for (auto* sink : sinks_) {
        if (sink) *sink << line << std::endl;
    }
}
