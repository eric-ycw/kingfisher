#ifndef MOVEPICK_H
#define MOVEPICK_H

#include "types.h"

enum MovePickStage { START_PICK, TT_PICK, NORMAL_GEN, NORMAL_PICK, NO_MOVES_LEFT };

uint16_t pickNextMove(const Board& b, const uint16_t& hashMove, int& stage, std::vector<ScoredMove>& moves, const int& ply, int& movesTried);

#endif
