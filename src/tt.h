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
	int depth, score, flag, ply, age = 0;
	int eval = NO_VALUE;
	Move move = NO_MOVE;
};

extern std::unordered_map<uint64_t, TTInfo> tt;

static constexpr int TTAgeLimit = 6;

int probeTT(const uint64_t& b, int depth, int alpha, int beta, int ply, SearchInfo& si, int& ttEval);
void storeTT(const uint64_t& b, int depth, int score, int flag, int eval, int ply);
void ageTT();

#endif