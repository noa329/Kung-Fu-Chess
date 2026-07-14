#include "PieceFactory.hpp"
#include "Pieces.hpp"

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
        case 'P': return std::make_shared<Pawn>(color); 
        default: return nullptr;
    }
}