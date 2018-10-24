#ifndef TT_H
#define TT_H

#include "move.h"
#include "types.h"

enum TTFlag { TT_EXACT, TT_ALPHA, TT_BETA };

struct TTInfo {
	TTInfo() {};
	TTInfo(int depthParam, int scoreParam, int flagParam) {
		depth = depthParam;
		score = scoreParam;
		flag = flagParam;
	};
	int depth, score, flag, age = 0;
	Move move = NO_MOVE;
};

extern std::unordered_map<uint64_t, TTInfo> tt;

static constexpr int TTAgeLimit = 6;

int probeTT(const uint64_t& b, int depth, int alpha, int beta, SearchInfo& si);
void storeTT(const uint64_t& b, int depth, int score, int flag);
void ageTT();

#endif