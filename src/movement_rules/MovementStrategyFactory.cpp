#include "MovementStrategyFactory.hpp"
#include "KingMovement.hpp"
#include "QueenMovement.hpp"
#include "RookMovement.hpp"
#include "BishopMovement.hpp"
#include "KnightMovement.hpp"
#include "PawnMovement.hpp"

const IMovementStrategy& getMovementStrategy(PieceKind kind) {
    static const KingMovement king;
    static const QueenMovement queen;
    static const RookMovement rook;
    static const BishopMovement bishop;
    static const KnightMovement knight;
    static const PawnMovement pawn;

    switch (kind) {
        case PieceKind::King:   return king;
        case PieceKind::Queen:  return queen;
        case PieceKind::Rook:   return rook;
        case PieceKind::Bishop: return bishop;
        case PieceKind::Knight: return knight;
        case PieceKind::Pawn:   return pawn;
    }
    return king; // בלתי-מגיע: כל ה-enum מכוסה למעלה
}
