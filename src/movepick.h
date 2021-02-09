#ifndef MOVEPICK_H
#define MOVEPICK_H

#include "types.h"

enum MovePickStage { START_PICK, TT_PICK, NORMAL_GEN, NORMAL_PICK, NO_MOVES_LEFT };

Move pickNextMove(const Board& b, const Move& hashMove, int& stage, std::vector<Move>& moves, const int& ply, int& movesSearched);

#endif
