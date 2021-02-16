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

struct qHashInfo {
	qHashInfo() {};
	int eval = NO_VALUE;
	uint64_t key = 0;
};

struct pHashInfo {
	pHashInfo() {};
	int staticEval = NO_VALUE;
	int color = NO_COLOR;
	uint64_t key = 0;
};

static constexpr int TTMaxEntry = 0xfffff;
static constexpr int TTAgeLimit = 6;
extern TTInfo tt[TTMaxEntry];

static constexpr int qHashMaxEntry = 0xfffff;
extern qHashInfo qhash[qHashMaxEntry];

static constexpr int pHashMaxEntry = 0xfff;
extern pHashInfo phash[pHashMaxEntry];

int probeTT(const uint64_t& key, const int& depth, const int& alpha, const int& beta, const int& ply, SearchInfo& si, int& ttEval);
Move probeHashMove(const uint64_t& key);
void storeTT(const uint64_t& key, const int& depth, const int& score, const int& flag, const int& eval, const int& ply, const Move& m);

int probePTT(const uint64_t& key, int depth);
void storePTT(const uint64_t& key, int depth, int nodes);

int probeQHash(const uint64_t& key);
void storeQHash(const uint64_t& key, const int& eval);

int probePawnHash(const uint64_t& key, const int& color);
void storePawnHash(const uint64_t& key, const int& staticEval, const int& color);

void ageTT();

#endif