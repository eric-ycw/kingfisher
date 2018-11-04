#ifndef ATTACKS_H
#define ATTACKS_H

#include "types.h"

static constexpr int bishopDirs[4] = { NE, NW, SE, SW };
static constexpr int rookDirs[4] = { N, E, S, W };
static constexpr int queenDirs[8] = { N, E, S, W, NE, NW, SE, SW };

extern uint64_t pawnAttacks[SQUARE_NUM][2];
extern uint64_t knightAttacks[SQUARE_NUM];
extern uint64_t bishopAttacks[SQUARE_NUM][4];
extern uint64_t rookAttacks[SQUARE_NUM][4];
extern uint64_t queenAttacks[SQUARE_NUM][8];
extern uint64_t kingAttacks[SQUARE_NUM];

void initPawnAttacks(const uint64_t& bb, int sqr);
void initKnightAttacks(const uint64_t& bb, int sqr);
void initBishopAttacks(int sqr);
void initRookAttacks(int sqr);
void initQueenAttacks(int sqr);
void initKingAttacks(const uint64_t& bb, int sqr);

void initAttacks();

uint64_t getBishopAttacks(const Board& b, const uint64_t& valid, int color, int sqr);
uint64_t getBishopAttacks(const uint64_t& occupied, int sqr);
uint64_t getRookAttacks(const Board& b, const uint64_t& valid, int color, int sqr);
uint64_t getRookAttacks(const uint64_t& occupied, int sqr);
uint64_t getQueenAttacks(const Board& b, const uint64_t& valid, int color, int sqr);

bool squareIsAttacked(const Board& b, int color, int sqr);
uint64_t squareAttackers(const Board& b, int color, int sqr);

int smallestAttacker(const Board& b, const uint64_t& attackers);

bool inCheck(const Board& b, int color);

#endif