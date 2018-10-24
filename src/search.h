#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "evaluate.h"
#include "move.h"
#include "types.h"

struct SearchInfo {
	int depth = 0;
	int seldepth = 0;
	int nodes = 0;
	int qnodes = 0;
	int score = 0;
	int failHigh = 0;
	int failHighFirst = 0;
	Move bestMove = NO_MOVE;
	double start = clock();
	int limit = 0;

	void operator=(const SearchInfo& si) {
		depth = si.depth;
		seldepth = si.seldepth;
		nodes = si.nodes;
		qnodes = si.qnodes;
		score = si.score;
		failHigh = si.failHigh;
		failHighFirst = si.failHighFirst;
		bestMove = si.bestMove;
		start = si.start;
		limit = si.limit;
	}

	void initTime(int limitParam) {
		start = clock();
		limit = limitParam;
	}

	void reset() {
		depth = 0;
		seldepth = 0;
		nodes = 0;
		qnodes = 0;
		score = 0;
		failHigh = 0;
		failHighFirst = 0;
		bestMove = NO_MOVE;
	}

	void print() {
		std::cout << "info score ";
		if (score > MATED_IN_MAX && score < MATE_IN_MAX) {
			std::cout << "cp " << score;
		}
		else {
			if (score > 0) {
				std::cout << "mate " << (MATE_SCORE - score + 1) / 2;
			}
			else {
				std::cout << "mate " << (MATE_SCORE + score) / 2;
			}
		}
		std::cout << " depth " << depth << " seldepth " << seldepth << " nodes " << nodes + qnodes;
		std::cout << " time " << (double)(clock() - start) / (CLOCKS_PER_SEC / 1000) << " ";
	}
};

static constexpr int aspirationMinDepth = 5;
static constexpr int aspirationWindow = 40;

static constexpr int nullMoveBaseR = 3;
static constexpr int nullMoveMinDepth = 3;
static constexpr float nullMovePhaseLimit = (float)0.2;

static constexpr int futilityMaxDepth = 6;
static constexpr int futilityMargin = 150;
static constexpr int deltaMargin = 125;

static constexpr int lateMoveBaseR = 1;
static constexpr int lateMoveMinDepth = 3;
static constexpr int lateMoveMinMove = 4;

static constexpr int hashMoveBonus = 500000;

static Move killers[2][MAX_PLY + 1];
static constexpr int killerBonus[4] = { 8000, 7500, 7250, 7000 };

static constexpr int historyMax = 6400;
static int historyMoves[2][6][SQUARE_NUM];

static constexpr int SEEValues[5] = { 
	// We use the same value for knight and bishop
	pieceValues[PAWN][MG], pieceValues[KNIGHT][MG], pieceValues[KNIGHT][MG], pieceValues[ROOK][MG], pieceValues[QUEEN][MG] 
};

void initSearch(SearchInfo& si);

bool timeOver(const SearchInfo& si);

void reduceHistory();

int scoreMove(const Board& b, const Move& m, int ply, float phase, const Move& hashMove);
std::vector<std::pair<Move, int>> scoreMoves(const Board& b, const std::vector<Move>& moves, int ply);

int scoreNoisyMove(const Board& b, const Move& m);
std::vector<std::pair<Move, int>> scoreNoisyMoves(const Board& b, const std::vector<Move>& moves);

int search(Board& b, int depth, int ply, int alpha, int beta, SearchInfo& si, bool allowNull);
int qsearch(Board& b, int ply, int alpha, int beta, SearchInfo& si);

int staticExchangeEvaluation(const Board& b, const Move& m, int threshold = 0);
int SEEMoveVal(const Board& b, const Move& m);

void iterativeDeepening(Board& b, SearchInfo& si, int timeLimit);

void printPV(Board b, int limit);

#endif