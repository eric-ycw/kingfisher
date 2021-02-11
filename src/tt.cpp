#include "board.h"
#include "move.h"
#include "search.h"
#include "tt.h"
#include "types.h"

TTInfo tt[TTMaxEntry];
PTTInfo ptt[TTMaxEntry];
qHashInfo qhash[qHashMaxEntry];

int probeTT(const uint64_t& key, int depth, int alpha, int beta, int ply, SearchInfo& si, int& ttEval) {
	auto& entry = tt[key & TTMaxEntry];
	if (entry.key == key) {
		if (entry.depth < depth) return NO_VALUE;
		ttEval = entry.eval;
		int score = entry.score;
		if (score == NO_VALUE) return NO_VALUE;
		if (score >= MATE_IN_MAX) score += entry.ply - ply;
		if (score <= MATED_IN_MAX) score += -entry.ply + ply;
		entry.age = 0;
		if (entry.flag == TT_EXACT) {
			return score;
		}
		else if ((entry.flag == TT_ALPHA) && (entry.score <= alpha)) {
			return alpha;
		}
		else if ((entry.flag == TT_BETA) && (entry.score >= beta)) {
			return beta;
		}
	}
	return NO_VALUE;
}

void storeTT(const uint64_t& key, int depth, int score, int flag, int eval, int ply, const Move& m) {
	auto& entry = tt[key & TTMaxEntry];
	entry.key = key;
	entry.depth = depth;
	entry.score = score;
	entry.flag = flag;
	if (eval != NO_VALUE) entry.eval = eval;
	entry.ply = ply;
	entry.move = m;
	entry.age = 0;
}

int probePTT(const uint64_t& key, int depth) {
	auto& entry = ptt[key & TTMaxEntry];
	return (entry.key == key && entry.depth == depth) ? entry.nodes : NO_NODES;
}


void storePTT(const uint64_t& key, int depth, int nodes) {
	auto& entry = ptt[key & TTMaxEntry];
	if (entry.nodes == NO_NODES) {
		entry.key = key;
		entry.depth = depth;
		entry.nodes = nodes;
	}
}

int probeQHash(const uint64_t& key) {
	auto& entry = qhash[key & qHashMaxEntry];
	return (entry.key == key) ? entry.eval : NO_VALUE;
}

int storeQHash(const uint64_t& key, int eval) {
	auto& entry = qhash[key & qHashMaxEntry];
	// Always replace
	entry.key = key;
	entry.eval = eval;
}

void ageTT() {
	for (int i = 0; i < TTMaxEntry; ++i) {
		if (tt[i].key != 0 && tt[i].age < TTAgeLimit) {
			tt[i].age++;
		}
		else {
			tt[i].key = 0;
		}
	}
}