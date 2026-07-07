#include "PieceFactory.h"
#include "Pieces.h"

std::shared_ptr<Piece> createPiece(const std::string& token) {
    if (token == ".") return nullptr;
    char color = token[0];
    char type = token[1];
    switch (type) {
        case 'K': return std::make_shared<King>(color);
        case 'Q': return std::make_shared<Queen>(color);
        case 'R': return std::make_shared<Rook>(color);
        case 'B': return std::make_shared<Bishop>(color);
        case 'N': return std::make_shared<Knight>(color);
        default: return nullptr; // 'P' - עוד לא מומש באיטרציה הזו
    }
}