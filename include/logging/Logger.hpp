#ifndef LOGGING_LOGGER_H
#define LOGGING_LOGGER_H
#include <string>
#include <vector>
#include <ostream>

// Structured, timestamped line logging shared by server/ and (Phase F)
// the shell client - two separate processes, each constructs its own
// instance. Deliberately not a singleton: there's no way to share
// in-memory state across processes anyway, and a later per-session
// GameSession might reasonably want its own Logger pointed at its own
// sinks, the same reasoning that kept EventBus off of a singleton too.
//
// Deliberately generic: just "write a timestamped line to every given
// sink." Event-specific formatting (a move happened, a score changed)
// stays in the caller's code (see GameSession's EventBus subscriptions)
// - the shell client will use this same class for entirely different
// kinds of lines (connect, command sent, error) that have nothing to do
// with EventBus events, so baking event-shaped formatting into Logger
// itself would be the wrong layer for it.
class Logger {
public:
    // `sinks` are non-owning - the caller keeps whatever std::ofstream it
    // opened alive for as long as this Logger is used. A null entry is
    // skipped (not dereferenced), so callers can pass a maybe-failed-to-open
    // file stream through without filtering it themselves. Pass {&std::cout}
    // for console-only, {&std::cout, &myFileStream} for both, or {} for a
    // Logger that silently does nothing (useful as a default/no-op state).
    explicit Logger(std::vector<std::ostream*> sinks);

    // Writes "[<timestamp>] <message>\n" to every non-null sink.
    void log(const std::string& message);

private:
    std::vector<std::ostream*> sinks_;
};
#endif
