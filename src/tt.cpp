#include "board.h"
#include "move.h"
#include "search.h"
#include "tt.h"
#include "types.h"

std::unordered_map<uint64_t, TTInfo> tt;

int probeTT(const uint64_t& key, int depth, int alpha, int beta, int ply, SearchInfo& si, int& ttEval) {
	auto it = tt.find(key);
	if (it != tt.end()) {
		ttEval = it->second.eval;
		int score = it->second.score;
		if (score >= MATE_IN_MAX) score += it->second.ply - ply;
		if (score <= MATED_IN_MAX) score += -it->second.ply + ply;
		it->second.age = 0;
		if (it->second.depth < depth) return NO_VALUE;
		if (it->second.flag == TT_EXACT) {
			return score;
		}
		else if ((it->second.flag == TT_ALPHA) && (it->second.score <= alpha)) {
			return alpha;
		}
		else if ((it->second.flag == TT_BETA) && (it->second.score >= beta)) {
			return beta;
		}
	}
	return NO_VALUE;
}

void storeTT(const uint64_t& key, int depth, int score, int flag, int eval, int ply) {
	auto it = tt.find(key);
	if (it != tt.end()) {
		it->second.depth = depth;
		it->second.score = score;
		it->second.flag = flag;
		if (eval != NO_VALUE) it->second.eval = eval;
		it->second.ply = ply;
		it->second.age = 0;
	}
	else {
		tt.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(depth, score, flag, eval, ply));
	}
}

void ageTT() {
	for (auto it = tt.begin(); it != tt.end();) {
		if (it->second.age < TTAgeLimit) {
			it->second.age++;
			it++;
		}
		else {
			it = tt.erase(it);
		}
	}
}