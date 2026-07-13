#ifndef PIECE_FACTORY_H
#define PIECE_FACTORY_H
#include <memory>
#include <string>
#include "Piece.hpp"

std::shared_ptr<Piece> createPiece(const std::string& token);
#endif