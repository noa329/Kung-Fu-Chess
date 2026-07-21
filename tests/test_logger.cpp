#include "doctest.h"
#include "Logger.hpp"
#include <sstream>

// logging layer: Logger is a plain instantiable class (not a singleton -
// server/ and the future shell client are separate processes, so there's
// no in-memory state to share between them anyway), deliberately generic
// ("write a timestamped line to every given sink"). Event-specific
// formatting stays in the caller (see GameSession's EventBus subscriptions
// in test_game_session.cpp) - the shell client will use this same class
// for entirely different kinds of lines that have nothing to do with
// EventBus events.

TEST_CASE("log() writes the message to every given sink") {
    std::ostringstream sinkA, sinkB;
    Logger logger({&sinkA, &sinkB});

    logger.log("hello");

    CHECK(sinkA.str().find("hello") != std::string::npos);
    CHECK(sinkB.str().find("hello") != std::string::npos);
}

TEST_CASE("log() prefixes the message with a bracketed timestamp") {
    std::ostringstream sink;
    Logger logger({&sink});

    logger.log("test message");
    std::string line = sink.str();

    REQUIRE(line.size() > 0);
    CHECK(line[0] == '[');
    CHECK(line.find("] test message") != std::string::npos);
}

TEST_CASE("multiple log() calls each produce their own line") {
    std::ostringstream sink;
    Logger logger({&sink});

    logger.log("first");
    logger.log("second");
    std::string content = sink.str();

    CHECK(content.find("first") != std::string::npos);
    CHECK(content.find("second") != std::string::npos);
    CHECK(content.find("first") < content.find("second"));
}

TEST_CASE("a Logger with no sinks does not throw") {
    Logger logger({});
    logger.log("nowhere to go, should be a no-op");
}

TEST_CASE("a null sink pointer in the list is skipped, not dereferenced") {
    std::ostringstream sink;
    Logger logger({nullptr, &sink});

    logger.log("survives a null sink");

    CHECK(sink.str().find("survives a null sink") != std::string::npos);
}
