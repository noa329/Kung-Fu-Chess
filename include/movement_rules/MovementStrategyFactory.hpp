#ifndef MOVEMENT_STRATEGY_FACTORY_H
#define MOVEMENT_STRATEGY_FACTORY_H
#include "IMovementStrategy.hpp"
#include "PieceKind.hpp"

const IMovementStrategy& getMovementStrategy(PieceKind kind);
#endif
