#include "movement_rules/MovementStrategyFactory.hpp"
#include "movement_rules/KingMovement.hpp"
#include "movement_rules/QueenMovement.hpp"
#include "movement_rules/RookMovement.hpp"
#include "movement_rules/BishopMovement.hpp"
#include "movement_rules/KnightMovement.hpp"
#include "movement_rules/PawnMovement.hpp"

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
