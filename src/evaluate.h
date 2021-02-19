#ifndef EVALUATE_H
#define EVALUATE_H

#include "types.h"

static inline int taperedScore(int mg, int eg, int phase) {
	return (mg * phase + eg * (256 - phase)) / 256;
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
static constexpr int knightPSQT[32] = {
	-90,-50,-30,-20,
	-50,-15,  0,  5,
	 -5, 20, 40, 45,
	-20, 10, 20, 40,
	-20, 10, 15, 40,
	-40,-12,  0,  5,
	-50,-25,-12, -5,
	-80,-30,-30,-30
};
static constexpr int bishopPSQT[32] = {
	-25,-10,-15,-25,
	-20, 12,  5,  0,
	-15, 10,  8,  0,
	-10, 20, 15,  5,
	 -5, 20, 15,  8,
	 -5, 20, 15,  8,
	-15, 20, 10,  0,
	-25,-10,-20,-25
};
static constexpr int rookPSQT[32] = {
	-20,-10, -5,  0,
	 -8,  5, 10, 15,
	-15, -5,  0,  0,
	-15, -5,  0,  0,
	-15, -5,  0,  0,
	-15, -8,  0,  0,
	-15, -8,  0,  0,
	-15,-12,-12, -8
};
static constexpr int queenPSQT[32] = {
	-30,-20,-10, -5,
	-10,  3,  3,  3,
	  0,  5,  5,  5,
	  0,  5,  8, 10,
	  0,  5,  8, 10,
	  0,  5,  5,  5,
	-10,  3,  3,  3,
	-30,-20,-10,  0
};
static constexpr int kingPSQT[32][2] = {
	{-99,-99}, {-99,-80}, {-99,-50}, {-99,-50},
	{-99,-70}, {-99,-50}, {-99,  0}, {-99,  0},
	{-99,-50}, {-99,-40}, {-99,  0}, {-99, 10},
	{-70,-40}, {-70,-25}, {-70, 10}, {-70, 20},
	{-50,-40}, {-50,-25}, {-60, 10}, {-60, 20},
	{-10,-20}, {-10,-15}, {-30,  0}, {-30, 10},
	{ -5,-20}, {-10,-15}, {-20, -5}, {-20, -5},
	{ 30,-80}, { 40,-60}, {-20,-30}, {-10,-30}
};

static constexpr int psqtFileTable[8] = { 0, 1, 2, 3, 3, 2, 1, 0 };

static constexpr int spacePhaseLimit = 128;

static constexpr int supportedPawnBonus = 20;
static constexpr int phalanxPawnBonus = 20;
static constexpr int doubledPawnPenalty = 48;
static constexpr int isolatedPawnPenalty = 14;

static constexpr int passedBonus[7][2] = {
	{0, 0}, {0, 20}, {5, 40}, {15, 60}, {50, 100}, {120, 180}, {250, 360}
};
static constexpr double passedBlockReduction = 2;

static constexpr int knightMobility[28]
{
	-50, -35, -10, 0, 5, 10, 15, 20, 25
};

static constexpr int bishopMobility[14]
{
	-50, -30, -5, 5, 10, 15, 20, 24, 28, 32, 34, 36, 38, 40
};

static constexpr int rookMobility[15]
{
	-40, -24, -12, -8, -4, 2, 7, 12, 16, 20, 23, 26, 29, 31, 34
};

static constexpr int queenMobility[28]
{
	-30, -20, -10, -5, 0, 5, 9, 12, 15, 18, 21, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 44, 45, 45, 46, 46, 46
};

static constexpr int kingAttackBonus = 12;
static constexpr int kingAttackerBonus[5] = { 20, 40, 35, 30, 10 };
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

int evaluateSpace(const Board& b, const uint64_t& safeSquares, const int& color, const int& phase);
int evaluateThreats(const Board&b, const uint64_t& defendedSquares, const uint64_t& attackedSquares, const int& color);

int evaluatePawns(const Board& b, uint64_t pawns, const uint64_t& enemyPawns, const uint64_t& enemyKingRing, int color, int phase);
int evaluateKnights(const Board& b, uint64_t knights, const uint64_t& safeSquares, const uint64_t& enemyKingRing, uint64_t& attackSquares);
int evaluateBishops(const Board& b, uint64_t bishops, const uint64_t& safeSquares, const uint64_t& enemyKingRing, uint64_t& attackSquares, int color);
int evaluateRooks(const Board& b, uint64_t rooks, const uint64_t& safeSquares, const uint64_t& enemyKingRing, uint64_t& attackSquares, int color);
int evaluateQueens(const Board& b, uint64_t queens, const uint64_t& safeSquares, const uint64_t& enemyKingRing, uint64_t& attackSquares, int color);
int evaluateKing(const Board& b, const int& sqr, const uint64_t& kingRing, const uint64_t& defendedSquares, const uint64_t& attackedSquares, int color, int phase);

int passed(const Board& b, int sqr, const uint64_t& enemyPawns, int color);
bool isPassed(const Board&b, int sqr, int color);

int openFile(const Board& b, int file);

int getPhase(const Board& b);

#endif
