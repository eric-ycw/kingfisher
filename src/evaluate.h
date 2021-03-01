#ifndef EVALUATE_H
#define EVALUATE_H

#include "types.h"

struct EvalInfo {
	int mg = 0;
	int eg = 0;

	int kingAttackCount[2] = { 0, 0 };
	int kingAttackerCount[2] = { 0, 0 };

	uint64_t kingRings[2];
	uint64_t pawns[2];
	uint64_t safeSquares[2]; // Squares not attacked by enemy pawns
	uint64_t attackSquares[2]; // Squares attacked by any of our pieces
};

static inline int taperedScore(int mg, int eg, int phase) {
	return (mg * phase + eg * (24 - phase)) / 24;
}

constexpr int materialSum = (400 * 2 + 420 * 2 + 650 * 2 + 1350) * 2;

constexpr int pieceValues[5][2] = {
	{100, 120}, {400, 380}, {420, 400}, {650, 600}, {1350, 1270}
};

static constexpr int pawnPSQT[32] = {
	0,  0,  0,  0,
	35, 40, 50, 50,
	25, 30, 35, 45,
	10, 10, 20, 25,
	 0,  0, 17, 22,
	 0,  0, 10, 12,
	 0,  0,  0,-10,
	 0,  0,  0,  0
};
static constexpr int knightPSQT[2][32] = {
	{-90,-50,-30,-20,
	-50,-15,  0,  5,
	 -5, 20, 40, 45,
	-20, 10, 20, 40,
	-20, 10, 15, 40,
	-40,-12,  0,  5,
	-50,-25,-12, -5,
	-80,-30,-30,-30},

	{-90,-50,-30,-20,
	-50,-15,  0,  5,
	 -5, 20, 40, 45,
	-20, 10, 20, 40,
	-20, 10, 15, 40,
	-40,-12,  0,  5,
	-50,-25,-12, -5,
	-80,-30,-30,-30}
};
static constexpr int bishopPSQT[2][32] = {
	{-25,-10,-15,-25,
	-20, 12,  5,  0,
	-15, 10,  8,  0,
	-10, 20, 15,  5,
	 -5, 20, 15,  8,
	 -5, 20, 15,  8,
	-15, 20, 10,  0,
	-25,-10,-20,-25},

	{-25,-10,-15,-25,
	-20, 12,  5,  0,
	-15, 10,  8,  0,
	-10, 20, 15,  5,
	 -5, 20, 15,  8,
	 -5, 20, 15,  8,
	-15, 20, 10,  0,
	-25,-10,-20,-25}
};
static constexpr int rookPSQT[2][32] = {
	{-20,-10, -5,  0,
	 -8,  5, 10, 15,
	-15, -5,  0,  0,
	-15, -5,  0,  0,
	-15, -5,  0,  0,
	-15, -8,  0,  0,
	-15, -8,  0,  0,
	-15,-12,-12, -8},

	{-20,-10, -5,  0,
	 -8,  5, 10, 15,
	-15, -5,  0,  0,
	-15, -5,  0,  0,
	-15, -5,  0,  0,
	-15, -8,  0,  0,
	-15, -8,  0,  0,
	-15,-12,-12, -8}
};
static constexpr int queenPSQT[2][32] = {
	{-30,-20,-10, -5,
	-10,  3,  3,  3,
	  0,  5,  5,  5,
	  0,  5,  8, 10,
	  0,  5,  8, 10,
	  0,  5,  5,  5,
	-10,  3,  3,  3,
	-30,-20,-10,  0},

	{-30,-20,-10, -5,
	-10,  3,  3,  3,
	  0,  5,  5,  5,
	  0,  5,  8, 10,
	  0,  5,  8, 10,
	  0,  5,  5,  5,
	-10,  3,  3,  3,
	-30,-20,-10,  0}
};
static constexpr int kingPSQT[2][32] = {
	{-99,-99,-99,-99,
	-99,-99,-99,-99,
	-99,-99,-99,-99,
	-70,-70,-70,-70,
	-50,-50,-60,-60,
	-10,-10,-30,-30,
	-5,-10,-20,-20,
	30,40,-20,-10},

	{-99,-80,-50,-50,
	-70,-50,0,0,
	-50,-40,0,10,
	-40,-25,10,20,
	-40,-25,10,20,
	-20,-15,0,10,
	-20,-15,-5,-5,
	-80,-60,-30,-30},
};

static constexpr int psqtFileTable[8] = { 0, 1, 2, 3, 3, 2, 1, 0 };

static constexpr int spacePhaseLimit = 12;

static constexpr int supportedPawnBonus = 20;
static constexpr int phalanxPawnBonus = 20;
static constexpr int doubledPawnPenalty = 48;
static constexpr int isolatedPawnPenalty = 14;

static constexpr int passedBonus[7][2] = {
	{0, 0}, {0, 20}, {5, 40}, {15, 60}, {50, 100}, {120, 180}, {250, 280}
};
static constexpr int passedBlockReduction = 2;
static constexpr int passedBlockedBonus[7][2] = {
	{passedBonus[0][0] / passedBlockReduction, passedBonus[0][1] / passedBlockReduction},
	{passedBonus[1][0] / passedBlockReduction, passedBonus[1][1] / passedBlockReduction},
	{passedBonus[2][0] / passedBlockReduction, passedBonus[2][1] / passedBlockReduction},
	{passedBonus[3][0] / passedBlockReduction, passedBonus[3][1] / passedBlockReduction},
	{passedBonus[4][0] / passedBlockReduction, passedBonus[4][1] / passedBlockReduction},
	{passedBonus[5][0] / passedBlockReduction, passedBonus[5][1] / passedBlockReduction},
	{passedBonus[6][0] / passedBlockReduction, passedBonus[6][1] / passedBlockReduction}
};

static constexpr int knightMobility[9]
{
	-80, -35, -15, -5, 5, 10, 15, 22, 35
};

static constexpr int bishopMobility[14]
{
	-70, -40, -15, -5, 6, 15, 20, 24, 26, 28, 30, 38, 50, 75
};

static constexpr int rookMobility[15]
{
	-100, -60, -25, -12, -5, 2, 7, 12, 16, 20, 23, 26, 29, 35, 50
};

static constexpr int queenMobility[28]
{
	-30, -20, -10, -5, 0, 5, 9, 12, 15, 18, 21, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 44, 45, 45, 46, 46, 46
};

static constexpr int kingAttackWeight[5] = { 0, 1, 1, 2, 4 };
static constexpr int kingAttackPenalty = 60;
static constexpr int kingAttackerWeight[8] = { 0, 0, 12, 18, 20, 22, 24, 24 };
static constexpr int weakSquarePenalty = 0; // FIXME: Elo loss
static constexpr int kingFilePenalty[3] = { 0, 15, 40 };

static constexpr int badBishopPenalty = 4;
static constexpr int bishopPairBonus = 35;

static constexpr int rookFileBonus[3] = { 0, 25, 40 };

static constexpr int tempoBonus = 20;

static constexpr int safePawnThreatBonus = 60;

static inline int psqtSquare(int square, int color) {
	assert(validSquare(square));
	return ((color == WHITE) ? 7 - square / 8 : square / 8) * 4 + psqtFileTable[square % 8];
}

int psqtScore(int piece, int sqr, int phase);

int evaluate(const Board& b, int color);

void evaluatePawns(const Board& b, EvalInfo& ei, int color);
void evaluateKnights(const Board& b, EvalInfo& ei, int color);
void evaluateBishops(const Board& b, EvalInfo& ei, int color);
void evaluateRooks(const Board& b, EvalInfo& ei, int color);
void evaluateQueens(const Board& b, EvalInfo& ei, int color);
void evaluateKing(const Board& b, EvalInfo& ei, int color);

void evaluateSpace(const Board& b, EvalInfo& ei, int color);
void evaluateThreats(const Board& b, EvalInfo& ei, int color);

int passed(const Board& b, int sqr, const uint64_t& enemyPawns, int color);
bool isPassed(const Board&b, int sqr, int color);

int openFile(const Board& b, int file);

int getPhase(const Board& b);

#endif
