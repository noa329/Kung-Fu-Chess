#ifndef RULE_ENGINE_H
#define RULE_ENGINE_H
#include <memory>
#include "Board.hpp"
#include "Piece.hpp"
#include "Position.hpp"

class RuleEngine {
    const Board& board;
public:
    explicit RuleEngine(const Board& b) : board(b) {}

    bool isLegal(const std::shared_ptr<Piece>& piece, const Position& from,
                 const Position& to, bool isCapture) const;
};
#endif
