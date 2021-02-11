#ifndef SEARCH_H
#define SEARCH_H

#include "board.h"
#include "evaluate.h"
#include "move.h"
#include "types.h"

static constexpr int FAIL_HIGH_MOVES = 6;
static constexpr int NEAR_LEAF_BOUNDARY = 8;

struct SearchInfo {
	int depth = 0;
	int seldepth = 0;
	int nodes = 0;
	int qnodes = 0;
	int score = 0;
	Move bestMove = NO_MOVE;
	double start = clock();
	int limit = 0;
	Move pv[MAX_PLY];

	// ### DEBUG ###
	int failHigh[3][FAIL_HIGH_MOVES];
	int hashCount = 0;
	int hashCut = 0;

	void operator=(const SearchInfo& si) {
		depth = si.depth;
		seldepth = si.seldepth;
		nodes = si.nodes;
		qnodes = si.qnodes;
		score = si.score;
		bestMove = si.bestMove;
		start = si.start;
		limit = si.limit;
		for (int i = 0; i < MAX_PLY; ++i) {
			if (si.pv[i] == NO_MOVE) break;
			pv[i] = si.pv[i];
		}

		// ### DEBUG ###
		for (int i = 0; i < FAIL_HIGH_MOVES; ++i) {
			for (int j = 0; j < 3; ++j) {
				failHigh[j][i] = si.failHigh[0][i];
			}
		}
		hashCount = si.hashCount;
		hashCut = si.hashCut;
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
		bestMove = NO_MOVE;
		for (auto& m : pv) m = NO_MOVE;

		// ### DEBUG ###
		for (int i = 0; i < FAIL_HIGH_MOVES; ++i) {
			for (int j = 0; j < 3; ++j) {
				failHigh[j][i] = 0;
			}
		}
		hashCount = 0;
		hashCut = 0;
	}

	void print() {
		std::cout << "info score ";
		if (score > MATED_IN_MAX && score < MATE_IN_MAX) {
			std::cout << "cp " << score;
		}
		else {
			std::cout << "mate " << (MATE_SCORE - abs(score)) / 2 + (score > 0);
		}
		std::cout << " depth " << depth << " seldepth " << seldepth << " nodes " << nodes + qnodes;
		std::cout << " time " << (double)(clock() - start) / (CLOCKS_PER_SEC / 1000);
		std::cout << " pv ";
		for (const auto& m : pv) {
			if (m == NO_MOVE) break;
			std::cout << toNotation(m) << " ";
		}
		std::cout << "\n";
	}

	void printSearchDebug() {
		std::cout << "\n+---+---+ ### NODES ### +---+---+\n";

		std::cout << "\nnodes: " << nodes << " | qnodes: " << qnodes << "\n";

		int nearLeafTotal = 0;
		int nearRootTotal = 0;
		int qTotal = 0;
		for (auto& i : failHigh[0]) nearLeafTotal += i;
		for (auto& i : failHigh[1]) nearRootTotal += i;
		for (auto& i : failHigh[2]) qTotal += i;

		std::cout << "\n+---+---+ ### MOVE ORDERING ### +---+---+\n";
		std::cout << "\nfail-high percentages by move order\n";
		std::cout << "near-leaf: ";
		for (auto& i : failHigh[0]) {
			std::cout << roundf((float)i / nearLeafTotal * 100 * 100) / 100 << "% | ";
		}
		std::cout << "\n";

		std::cout << "near-root: ";
		for (auto& i : failHigh[1]) {
			std::cout << roundf((float)i / nearRootTotal * 100 * 100) / 100 << "% | ";
		}
		std::cout << "\n";

		std::cout << "qsearch: ";
		for (auto& i : failHigh[2]) {
			std::cout << roundf((float)i / qTotal * 100 * 100) / 100 << "% | ";
		}
		std::cout << "\n";

		std::cout << "hash cut percentage : ";
		std::cout << roundf((float)hashCut / hashCount * 100 * 100) / 100 << "%\n";
		std::cout << "\n";
	}
};

static constexpr int aspirationMinDepth = 5;
static constexpr int aspirationWindow = 35;

static constexpr int nullMoveBaseR = 2;
static constexpr int nullMoveMinDepth = 3;
static constexpr float nullMovePhaseLimit = (float)0.2;

static constexpr int futilityMaxDepth = 8;
static constexpr int futilityMargin = 150;
static constexpr int deltaMargin = 125;

static constexpr int lateMoveMinDepth = 3;

static constexpr int lateMoveRTable[64] = {
	0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
	3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4
};

static constexpr int lateMovePruningMaxDepth = 3;
static constexpr int lateMovePruningMove = 7;

static constexpr int hashMoveBonus = INT_MAX - 1;

static Move killers[2][MAX_PLY + 1];
static constexpr int killerBonus[4] = { -1, -2, -3, -4 };

static constexpr int historyMax = 5000;
static constexpr int historyMaxDepth = 8;
static int historyMoves[2][6][SQUARE_NUM];

static constexpr int SEEValues[5] = { 
	// We use the same value for knight and bishop
	pieceValues[PAWN][MG], pieceValues[KNIGHT][MG], pieceValues[KNIGHT][MG], pieceValues[ROOK][MG], pieceValues[QUEEN][MG] 
};

void initSearch(SearchInfo& si);

bool timeOver(const SearchInfo& si, const bool ignoreDepth);

void reduceHistory();

int scoreMove(const Board& b, const Move& m, int ply, float phase, const Move& hashMove, bool& ageHistory);
std::vector<Move> scoreMoves(const Board& b, const std::vector<Move>& moves, int ply, const Move& hashMove, bool& ageHistory);

int scoreNoisyMove(const Board& b, const Move& m);
std::vector<Move> scoreNoisyMoves(const Board& b, const std::vector<Move>& moves);

int search(Board& b, int depth, int ply, int alpha, int beta, SearchInfo& si, Move(&ppv)[MAX_PLY], bool allowNull);
int qsearch(Board& b, int ply, int alpha, int beta, SearchInfo& si, Move (&ppv)[MAX_PLY]);

int staticExchangeEvaluation(const Board& b, const Move& m, int threshold = 0);
int SEEMoveVal(const Board& b, const Move& m);
int greatestTacticalGain(const Board& b);

void iterativeDeepening(Board& b, SearchInfo& si, int timeLimit);

#endif