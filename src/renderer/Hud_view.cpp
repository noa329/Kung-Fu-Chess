#include "Hud_view.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace {

std::string formatTime(long long ms) {
    long long minutes = ms / 60000;
    long long seconds = (ms / 1000) % 60;
    long long millis = ms % 1000;
    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << minutes << ':'
        << std::setw(2) << std::setfill('0') << seconds << '.'
        << std::setw(3) << std::setfill('0') << millis;
    return oss.str();
}

} // namespace

int HudView::visibleRows(int panelH) {
    int contentH = panelH - PANEL_HEADER_H - ROW_H; // header line + column-title row
    return std::max(0, contentH / ROW_H);
}

int HudView::clampScrollOffset(int offset, size_t totalRows, int visible) {
    int maxOffset = std::max(0, static_cast<int>(totalRows) - visible);
    if (offset < 0) offset = 0;
    if (offset > maxOffset) offset = maxOffset;
    return offset;
}

void HudView::drawBar(Img& canvas, int y, int h, int canvasW, const std::string& name, int score) const {
    canvas.rectangle(0, y, canvasW, h, cv::Scalar(40, 40, 40, 255), -1);
    std::string text = "Name: " + name + "   Score: " + std::to_string(score);
    int textX = std::max(10, canvasW / 2 - static_cast<int>(text.size()) * 4);
    canvas.put_text(text, textX, y + h / 2 + 6, 0.6, cv::Scalar(255, 255, 255, 255), 1);
}

void HudView::drawPanel(Img& canvas, int x, int y, int h, const std::string& header,
                         const std::vector<MoveRecord>& moves, int scrollOffset) const {
    int panelW = (header == "Black") ? LEFT_PANEL_W : RIGHT_PANEL_W;
    canvas.rectangle(x, y, panelW, h, cv::Scalar(50, 50, 50, 255), -1);
    canvas.put_text(header, x + 10, y + 18, 0.7, cv::Scalar(255, 255, 255, 255), 2);
    canvas.put_text("Time", x + 10, y + PANEL_HEADER_H + 16, 0.45, cv::Scalar(200, 200, 200, 255), 1);
    canvas.put_text("Move", x + 90, y + PANEL_HEADER_H + 16, 0.45, cv::Scalar(200, 200, 200, 255), 1);

    int visible = visibleRows(h);
    int total = static_cast<int>(moves.size());
    int start = std::max(0, total - visible - scrollOffset);
    int end = std::min(total, start + visible);

    int rowY = y + PANEL_HEADER_H + ROW_H + 12;
    for (int i = start; i < end; ++i) {
        const auto& m = moves[i];
        canvas.put_text(formatTime(m.atMs), x + 10, rowY, 0.4, cv::Scalar(220, 220, 220, 255), 1);
        canvas.put_text(m.notation, x + 90, rowY, 0.4, cv::Scalar(220, 220, 220, 255), 1);
        rowY += ROW_H;
    }
}

void HudView::drawCoordinates(Img& canvas, int boardW, int boardH) const {
    int originX = boardOriginX();
    int originY = boardOriginY();
    int cellW = boardW / 8;
    int cellH = boardH / 8;

    for (int col = 0; col < 8; ++col) {
        std::string file(1, char('a' + col));
        int cx = originX + col * cellW + cellW / 2 - 4;
        canvas.put_text(file, cx, originY - 6, 0.45, cv::Scalar(230, 230, 230, 255), 1);
        canvas.put_text(file, cx, originY + boardH + 16, 0.45, cv::Scalar(230, 230, 230, 255), 1);
    }
    for (int row = 0; row < 8; ++row) {
        std::string rank = std::to_string(8 - row);
        int ry = originY + row * cellH + cellH / 2 + 5;
        canvas.put_text(rank, LEFT_PANEL_W + 4, ry, 0.45, cv::Scalar(230, 230, 230, 255), 1);
        canvas.put_text(rank, originX + boardW + 4, ry, 0.45, cv::Scalar(230, 230, 230, 255), 1);
    }
}

void HudView::drawGameOverBanner(Img& canvas, const GameSnapshot& snap, int boardW, int boardH) const {
    int bannerW = boardW;
    int bannerH = boardH / 4;
    int bx = boardOriginX();
    int by = boardOriginY() + boardH / 2 - bannerH / 2;

    // Partially-transparent black (alpha 170/255) so board squares underneath
    // stay faintly visible; draw_on() below blends per-pixel using this alpha.
    Img banner(bannerW, bannerH, cv::Scalar(0, 0, 0, 170));

    const std::string title = "GAME OVER";
    const std::string subtitle = snap.result;
    const double titleScale = 1.3;
    const int titleThickness = 3;
    const double subtitleScale = 0.8;
    const int subtitleThickness = 2;
    const int lineGap = 14;

    auto [titleW, titleH] = banner.text_size(title, titleScale, titleThickness);
    auto [subW, subH] = banner.text_size(subtitle, subtitleScale, subtitleThickness);

    int blockH = titleH + lineGap + subH;
    int titleBaselineY = (bannerH - blockH) / 2 + titleH;
    int subBaselineY = titleBaselineY + lineGap + subH;

    banner.put_text(title, (bannerW - titleW) / 2, titleBaselineY, titleScale,
                     cv::Scalar(255, 255, 255, 255), titleThickness);
    banner.put_text(subtitle, (bannerW - subW) / 2, subBaselineY, subtitleScale,
                     cv::Scalar(255, 255, 255, 255), subtitleThickness);

    banner.draw_on(canvas, bx, by);
}

Img HudView::compose(const Img& boardFrame, const GameSnapshot& snap) {
    int boardW = boardFrame.width();
    int boardH = boardFrame.height();
    lastBoardW_ = boardW;
    lastBoardH_ = boardH;
    lastBlackCount_ = snap.blackMoves.size();
    lastWhiteCount_ = snap.whiteMoves.size();

    int canvasW = LEFT_PANEL_W + RANK_GUTTER_W + boardW + RANK_GUTTER_W + RIGHT_PANEL_W;
    int canvasH = TOP_BAR_H + FILE_LABEL_H + boardH + FILE_LABEL_H + BOTTOM_BAR_H;

    Img canvas(canvasW, canvasH, cv::Scalar(60, 60, 60, 255));

    boardFrame.draw_on(canvas, boardOriginX(), boardOriginY());

    if (snap.gameOver) {
        drawGameOverBanner(canvas, snap, boardW, boardH);
    }

    // Row 0 is black's back rank, so black reads as the "top" side.
    drawBar(canvas, 0, TOP_BAR_H, canvasW, snap.blackName, snap.blackScore);
    drawBar(canvas, canvasH - BOTTOM_BAR_H, BOTTOM_BAR_H, canvasW, snap.whiteName, snap.whiteScore);

    int visible = visibleRows(boardH);
    blackScrollOffset_ = clampScrollOffset(blackScrollOffset_, lastBlackCount_, visible);
    whiteScrollOffset_ = clampScrollOffset(whiteScrollOffset_, lastWhiteCount_, visible);

    drawPanel(canvas, 0, boardOriginY(), boardH, "Black", snap.blackMoves, blackScrollOffset_);
    drawPanel(canvas, canvasW - RIGHT_PANEL_W, boardOriginY(), boardH, "White", snap.whiteMoves, whiteScrollOffset_);

    drawCoordinates(canvas, boardW, boardH);

    return canvas;
}

void HudView::handleScroll(int mouseX, int mouseY, int wheelDelta) {
    (void)mouseY;
    if (wheelDelta == 0) return;
    int delta = (wheelDelta > 0) ? 1 : -1;
    int visible = visibleRows(lastBoardH_);
    int canvasW = LEFT_PANEL_W + RANK_GUTTER_W + lastBoardW_ + RANK_GUTTER_W + RIGHT_PANEL_W;

    if (mouseX < LEFT_PANEL_W) {
        blackScrollOffset_ = clampScrollOffset(blackScrollOffset_ + delta, lastBlackCount_, visible);
    } else if (mouseX > canvasW - RIGHT_PANEL_W) {
        whiteScrollOffset_ = clampScrollOffset(whiteScrollOffset_ + delta, lastWhiteCount_, visible);
    }
}
