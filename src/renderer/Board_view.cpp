#include "Board_view.hpp"
#include <cctype>
#include <cmath>
#include <iostream>

bool BoardView::init(const std::string& assets_root, const std::string& piece_set) {
    boardImg_.read(assets_root + "/board.png");
    if (!boardImg_.is_loaded()) {
        return false;
    }

    cellSize_ = std::min(boardImg_.width(), boardImg_.height()) / rows_;
    piecesDir_ = assets_root + "/" + piece_set;
    cells_.clear();
    return true;
}

std::string BoardView::tokenToSpriteCode(const std::string& token) {
    if (token.size() != 2) {
        return "";
    }
    char color = static_cast<char>(std::toupper(static_cast<unsigned char>(token[0])));
    char type = token[1];
    return std::string(1, type) + std::string(1, color);
}

void BoardView::syncFromSnapshot(const GameSnapshot& snap) {
    for (int r = 0; r < static_cast<int>(snap.boardTokens.size()); ++r) {
        for (int c = 0; c < static_cast<int>(snap.boardTokens[r].size()); ++c) {
            const std::string& token = snap.boardTokens[r][c];
            auto key = std::make_pair(r, c);

            if (token == ".") {
                cells_.erase(key);
                continue;
            }

            const std::string spriteCode = tokenToSpriteCode(token);
            auto it = cells_.find(key);

            if (it == cells_.end() || it->second.pieceCode != spriteCode) {
                // New piece here (either the square was empty, or a
                // different piece just landed on it) - (re)load its sprites.
                AnimatedCell cell;
                cell.pieceCode = spriteCode;
                if (!cell.anim.init(piecesDir_ + "/" + spriteCode, {cellSize_, cellSize_})) {
                    std::cerr << "Warning: failed to load sprites for " << spriteCode
                              << " from " << piecesDir_ << std::endl;
                    continue;
                }
                cells_[key] = std::move(cell);
                it = cells_.find(key);
            }

            const std::string& targetState =
                (r < static_cast<int>(snap.cellStates.size()) && c < static_cast<int>(snap.cellStates[r].size()))
                    ? snap.cellStates[r][c]
                    : std::string("idle");

            if (it->second.anim.state() != targetState) {
                it->second.anim.setState(targetState);
            }
        }
    }
}

void BoardView::update(int dt_ms) {
    for (auto& [key, cell] : cells_) {
        cell.anim.update(dt_ms);
    }
}

Img BoardView::render(const GameSnapshot& snap) {
    Img frame = boardImg_.clone();

    for (auto& [key, cell] : cells_) {
        const int r = key.first;
        const int c = key.second;

        // Default: draw at this cell's fixed pixel position.
        double px = c * cellSize_;
        double py = r * cellSize_;

        const bool inBounds = r < static_cast<int>(snap.cellStates.size()) &&
                               c < static_cast<int>(snap.cellStates[r].size());
        if (inBounds && snap.cellStates[r][c] == "move") {
            const Position& to = snap.moveTargets[r][c];
            const double t = snap.moveProgress[r][c];
            const double toX = to.col * cellSize_;
            const double toY = to.row * cellSize_;
            px = px + (toX - px) * t;
            py = py + (toY - py) * t;
        }

        cell.anim.currentFrame().draw_on(frame, static_cast<int>(std::round(px)), static_cast<int>(std::round(py)));
    }

    if (snap.selected.row >= 0 && snap.selected.col >= 0) {
        frame.rectangle(snap.selected.col * cellSize_, snap.selected.row * cellSize_,
                         cellSize_, cellSize_, cv::Scalar(0, 255, 255, 255), 3);
    }

    for (const auto& flash : snap.captureFlashes) {
        const double t = flash.progress;
        const int cx = static_cast<int>(std::round(flash.at.col * cellSize_ + cellSize_ / 2.0));
        const int cy = static_cast<int>(std::round(flash.at.row * cellSize_ + cellSize_ / 2.0));
        double radius = cellSize_ * (0.30 + 0.25 * t);
        int thickness = 2 + static_cast<int>(std::round(3 * t));
        if (flash.wasKing) {
            radius *= 1.15;
            thickness += 1;
        }
        // Orange (0,165,255) fading to red (0,0,255) as the flash decays.
        int green = static_cast<int>(std::round(165 * (1.0 - t)));
        frame.circle(cx, cy, static_cast<int>(std::round(radius)), cv::Scalar(0, green, 255, 255), thickness);
    }

    return frame;
}
