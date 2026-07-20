#pragma once

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include "img.hpp"
#include "GameEngine.hpp"

/**
 * Pure compositing layer around an already-rendered board frame: draws
 * name/score bars, per-player move-log panels (with mouse-wheel scroll),
 * and file/rank coordinate labels. Owns no game logic and never touches
 * BoardView/RealTimeArbiter/GameEngine internals - it only reads a
 * GameSnapshot and an Img handed to it by the caller.
 */
class HudView {
public:
    /** Compose a wider canvas: board frame inset, HUD panels/bars around it. */
    Img compose(const Img& boardFrame, const GameSnapshot& snap);

    /** Route a mouse-wheel event here (from main's onMouse) to scroll a move-log panel. */
    void handleScroll(int mouseX, int mouseY, int wheelDelta);

    /**
     * Hook for the composition root's onGameLifecycle("end", ...) subscriber
     * (main.cpp) - HudView has no event_bus dependency itself, it's just
     * handed the result string directly. Currently an inert stub: it only
     * records that the event fired. compose() still draws the game-over
     * banner purely from GameSnapshot::gameOver/result, unchanged - wiring
     * an actual one-shot animation (e.g. a fade-in) into compose() is
     * follow-up work, not part of this pass.
     */
    void playEndAnimation(const std::string& result);

    /** Pixel offset of the board frame within the composed canvas - main.cpp
     *  needs this to translate raw window clicks back to board-local pixels. */
    int boardOriginX() const { return LEFT_PANEL_W + RANK_GUTTER_W; }
    int boardOriginY() const { return TOP_BAR_H + FILE_LABEL_H; }

private:
    static constexpr int LEFT_PANEL_W = 200;
    static constexpr int RIGHT_PANEL_W = 200;
    static constexpr int TOP_BAR_H = 40;
    static constexpr int BOTTOM_BAR_H = 40;
    static constexpr int RANK_GUTTER_W = 24;
    static constexpr int FILE_LABEL_H = 24;
    static constexpr int ROW_H = 20;
    static constexpr int PANEL_HEADER_H = 24;

    int blackScrollOffset_ = 0;
    int whiteScrollOffset_ = 0;

    // Set by playEndAnimation() - not read anywhere yet (see its doc comment).
    bool endAnimationTriggered_ = false;
    std::string endAnimationResult_;

    // Cached from the last compose() call so handleScroll (fired from the
    // mouse callback, between frames) knows panel bounds/row counts without
    // needing a GameSnapshot of its own.
    int lastBoardW_ = 0;
    int lastBoardH_ = 0;
    size_t lastBlackCount_ = 0;
    size_t lastWhiteCount_ = 0;

    void drawBar(Img& canvas, int y, int h, int canvasW, const std::string& name, int score) const;
    void drawPanel(Img& canvas, int x, int y, int h, const std::string& header,
                   const std::vector<MoveRecord>& moves, int scrollOffset) const;
    void drawCoordinates(Img& canvas, int boardW, int boardH) const;
    void drawGameOverBanner(Img& canvas, const GameSnapshot& snap, int boardW, int boardH) const;

    static int visibleRows(int panelH);
    static int clampScrollOffset(int offset, size_t totalRows, int visible);
};
