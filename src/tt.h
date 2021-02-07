#ifndef TT_H
#define TT_H

#include "move.h"
#include "types.h"

enum TTFlag { TT_EXACT, TT_ALPHA, TT_BETA };

struct TTInfo {
	TTInfo() {};
	TTInfo(int depthParam, int scoreParam, int flagParam, int evalParam, int plyParam) {
		depth = depthParam;
		score = scoreParam;
		flag = flagParam;
		eval = evalParam;
		ply = plyParam;
	};
	int depth, flag, ply, age = 0;
	int score, eval = NO_VALUE;
	Move move = NO_MOVE;
	uint64_t key = 0;
};

struct PTTInfo {
	PTTInfo() {};
	int depth = 0;
	int nodes = NO_NODES;
	uint64_t key = 0;
};

static constexpr int TTMaxEntry = 0xfffff;
static constexpr int TTAgeLimit = 6;

extern TTInfo tt[TTMaxEntry];

int probeTT(const uint64_t& b, int depth, int alpha, int beta, int ply, SearchInfo& si, int& ttEval);
int probePTT(const uint64_t& b, int depth);
void storeTT(const uint64_t& b, int depth, int score, int flag, int eval, int ply, const Move& m);
void storePTT(const uint64_t& b, int depth, int nodes);
void ageTT();

#endif