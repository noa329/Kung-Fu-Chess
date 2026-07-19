#pragma once

#include <string>
#include <map>
#include <memory>
#include <opencv2/opencv.hpp>
#include "img.hpp"
#include "Piece_animator.hpp"
#include "GameEngine.hpp"

/**
 * Pure graphics view over a GameEngine: it owns no game logic of its own -
 * every frame it reads a GameSnapshot (board tokens, per-cell animation
 * state, current selection) and draws it. Clicks are not handled here;
 * route them through Controller (see Controller::handleClick/handleJump)
 * so the real rule engine decides what's legal.
 */
class BoardView {
public:
    /**
     * @param assets_root path to the kungfu-graphics folder (contains board.png
     *                    and the pieces1/pieces2 folders)
     * @param piece_set   which sprite set to use, e.g. "pieces1" or "pieces2"
     */
    bool init(const std::string& assets_root, const std::string& piece_set);

    /**
     * Make sure every occupied cell in the snapshot has an animator loaded
     * for the right piece, and push each cell's target state ("idle" /
     * "move" / "jump") into that animator. Call once per frame, before update().
     */
    void syncFromSnapshot(const GameSnapshot& snap);

    /** Advance every on-board piece's animation. Call once per frame. */
    void update(int dt_ms);

    /** Render the current frame (background + pieces + selection highlight + capture flashes). */
    Img render(const GameSnapshot& snap);

    int cellSize() const { return cellSize_; }

    /** Debug/test-only: the cached animator's actual current state at a
     *  board cell, or "" if nothing is cached there. Lets headless probes
     *  verify BoardView really applied a state, not just infer it from
     *  rendered pixels. */
    std::string debugAnimatorState(int r, int c) const {
        auto it = cells_.find({r, c});
        return it != cells_.end() ? it->second.anim.state() : std::string();
    }

private:
    struct AnimatedCell {
        std::string pieceCode;   // sprite-folder code, e.g. "PW"
        PieceAnimator anim;
    };

    Img boardImg_;
    int rows_ = 8;
    int cols_ = 8;
    int cellSize_ = 0; // board.png is expected to be square-ish; one size is used for both axes

    std::string piecesDir_;
    // Keyed by "row,col" - keeps each cell's animator alive across frames
    // (and across the piece "teleporting" from its origin cell to its
    // destination cell once the real engine resolves the move).
    std::map<std::pair<int, int>, AnimatedCell> cells_;

    static std::string tokenToSpriteCode(const std::string& token);
};
