#include "doctest.h"
#include "RealTimeArbiter.hpp"
#include "Board.hpp"
#include "Pieces.hpp"

// שכבת RealTimeArbiter: אובייקטי Motion פעילים, קידום זמן מדומה,
// פתרון הגעה, ואירועי אכילה. אין כאן חוקיות שחמט, קליקים, רינדור, או
// פירוש סקריפט - וגם לא החלטת "המשחק נגמר" (זה תפקיד GameEngine,
// שמקבל בחזרה CaptureEvent ומחליט מה לעשות איתו).

TEST_CASE("scheduling a move marks the source as pending immediately") {
    Board board;
    board.setGrid({{"wR", ".", "."}});
    RealTimeArbiter arbiter(board);
    auto rook = board.getCell(0, 0);
    arbiter.scheduleMove({0,0}, {0,2}, rook, /*isCapture=*/false);
    CHECK(arbiter.hasPendingMoveFrom({0,0}) == true);
    CHECK(arbiter.hasPendingMoveTo({0,2}) == true);
}

TEST_CASE("advancing time before arrival leaves the board untouched") {
    Board board;
    board.setGrid({{"wR", ".", "."}});
    RealTimeArbiter arbiter(board);
    auto rook = board.getCell(0, 0);
    arbiter.scheduleMove({0,0}, {0,2}, rook, /*isCapture=*/false); // distance 2 -> 2000ms
    arbiter.advance(500);
    CHECK(board.getCell(0, 0).get() != nullptr);
    CHECK(board.getCell(0, 2).get() == nullptr);
    CHECK(arbiter.hasPendingMoveFrom({0,0}) == true);
}

TEST_CASE("advancing time past arrival resolves the move on the board") {
    Board board;
    board.setGrid({{"wR", ".", "."}});
    RealTimeArbiter arbiter(board);
    auto rook = board.getCell(0, 0);
    arbiter.scheduleMove({0,0}, {0,2}, rook, /*isCapture=*/false);
    arbiter.advance(2000);
    CHECK(board.getCell(0, 0).get() == nullptr);
    CHECK(board.getCell(0, 2).get() == rook.get());
    CHECK(arbiter.hasPendingMoveFrom({0,0}) == false);
}

TEST_CASE("a capturing move always takes the fixed capture duration") {
    Board board;
    board.setGrid({{"wR", ".", ".", ".", "bP"}});
    RealTimeArbiter arbiter(board);
    auto rook = board.getCell(0, 0);
    arbiter.scheduleMove({0,0}, {0,4}, rook, /*isCapture=*/true);
    arbiter.advance(999);
    CHECK(board.getCell(0, 4).get() != rook.get());
    arbiter.advance(1);
    CHECK(board.getCell(0, 4).get() == rook.get());
}

TEST_CASE("jumping marks a piece airborne until the jump duration elapses") {
    Board board;
    board.setGrid({{"wN", "."}});
    RealTimeArbiter arbiter(board);
    auto knight = board.getCell(0, 0);
    arbiter.scheduleJump({0,0}, knight);
    CHECK(arbiter.isAirborne({0,0}) == true);
    arbiter.advance(999);
    CHECK(arbiter.isAirborne({0,0}) == true);
    arbiter.advance(1);
    CHECK(arbiter.isAirborne({0,0}) == false);
}

TEST_CASE("a pawn reaching the last row is promoted to a queen on arrival") {
    Board board;
    board.setGrid({{"."}, {"wP"}});
    RealTimeArbiter arbiter(board);
    auto pawn = board.getCell(1, 0);
    arbiter.scheduleMove({1,0}, {0,0}, pawn, /*isCapture=*/false);
    arbiter.advance(1000);
    CHECK(board.getCell(0, 0)->toString() == "wQ");
}

TEST_CASE("capturing an airborne defender removes the attacker instead of landing") {
    Board board;
    board.setGrid({{"wR", "bN"}});
    RealTimeArbiter arbiter(board);
    auto rook = board.getCell(0, 0);
    auto knight = board.getCell(0, 1);
    arbiter.scheduleJump({0,1}, knight);          // ההגנה קופצת באוויר
    arbiter.scheduleMove({0,0}, {0,1}, rook, true); // התוקף מנסה לאכול
    arbiter.advance(1000);
    CHECK(board.getCell(0, 0).get() == nullptr);  // התוקף נמחק
    CHECK(board.getCell(0, 1).get() == knight.get());   // המגן נשאר במקומו
}

TEST_CASE("capturing a king is reported back as a capture event") {
    Board board;
    board.setGrid({{"wR", "bK"}});
    RealTimeArbiter arbiter(board);
    auto rook = board.getCell(0, 0);
    arbiter.scheduleMove({0,0}, {0,1}, rook, /*isCapture=*/true);
    auto events = arbiter.advance(1000);
    CHECK(events.size() == 1);
    CHECK(events[0].wasKing == true);
    CHECK(board.getCell(0, 1).get() == rook.get()); // המלך בכל זאת מוחלף בכלי התוקף
}

TEST_CASE("a capture event reports the captured piece's kind") {
    Board board;
    board.setGrid({{"wR", "bN"}});
    RealTimeArbiter arbiter(board);
    auto rook = board.getCell(0, 0);
    arbiter.scheduleMove({0,0}, {0,1}, rook, /*isCapture=*/true);
    auto events = arbiter.advance(1000);
    CHECK(events.size() == 1);
    CHECK(events[0].capturedKind == PieceKind::Knight);
    CHECK(events[0].capturedColor == 'b');
}

TEST_CASE("a landed move puts the destination into long rest until it expires") {
    Board board;
    board.setGrid({{"wR", ".", "."}});
    RealTimeArbiter arbiter(board);
    auto rook = board.getCell(0, 0);
    arbiter.scheduleMove({0,0}, {0,2}, rook, /*isCapture=*/false); // distance 2 -> 2000ms
    arbiter.advance(2000); // lands at (0,2)
    bool isLongRest = false;
    CHECK(arbiter.isResting({0,2}, &isLongRest) == true);
    CHECK(isLongRest == true);
    arbiter.advance(799);
    CHECK(arbiter.isResting({0,2}) == true);
    arbiter.advance(1);
    CHECK(arbiter.isResting({0,2}) == false);
}

TEST_CASE("a completed jump puts the piece into short rest until it expires") {
    Board board;
    board.setGrid({{"wN", "."}});
    RealTimeArbiter arbiter(board);
    auto knight = board.getCell(0, 0);
    arbiter.scheduleJump({0,0}, knight);
    arbiter.advance(1000); // jump ends
    bool isLongRest = true;
    CHECK(arbiter.isResting({0,0}, &isLongRest) == true);
    CHECK(isLongRest == false);
    arbiter.advance(499);
    CHECK(arbiter.isResting({0,0}) == true);
    arbiter.advance(1);
    CHECK(arbiter.isResting({0,0}) == false);
}

TEST_CASE("setRestDurations overrides the long-rest cooldown length") {
    Board board;
    board.setGrid({{"wR", ".", "."}});
    RealTimeArbiter arbiter(board);
    arbiter.setRestDurations(/*longRestMs=*/300, /*shortRestMs=*/150);
    auto rook = board.getCell(0, 0);
    arbiter.scheduleMove({0,0}, {0,2}, rook, /*isCapture=*/false); // distance 2 -> 2000ms
    arbiter.advance(2000); // lands at (0,2)
    CHECK(arbiter.isResting({0,2}) == true);
    arbiter.advance(299);
    CHECK(arbiter.isResting({0,2}) == true);
    arbiter.advance(1);
    CHECK(arbiter.isResting({0,2}) == false);
}

TEST_CASE("setRestDurations overrides the short-rest (jump) cooldown length") {
    Board board;
    board.setGrid({{"wN", "."}});
    RealTimeArbiter arbiter(board);
    arbiter.setRestDurations(/*longRestMs=*/300, /*shortRestMs=*/150);
    auto knight = board.getCell(0, 0);
    arbiter.scheduleJump({0,0}, knight);
    arbiter.advance(1000); // jump ends
    CHECK(arbiter.isResting({0,0}) == true);
    arbiter.advance(149);
    CHECK(arbiter.isResting({0,0}) == true);
    arbiter.advance(1);
    CHECK(arbiter.isResting({0,0}) == false);
}

TEST_CASE("a blocked move (friendly occupant at destination) does not put anything into rest") {
    Board board;
    board.setGrid({{"wR", ".", "wP"}});
    RealTimeArbiter arbiter(board);
    auto rook = board.getCell(0, 0);
    arbiter.scheduleMove({0,0}, {0,2}, rook, /*isCapture=*/false);
    arbiter.advance(2000);
    CHECK(arbiter.isResting({0,0}) == false);
    CHECK(arbiter.isResting({0,2}) == false);
}
