#include "board.h"
#include "move.h"
#include "search.h"
#include "tt.h"
#include "types.h"

std::unordered_map<uint64_t, TTInfo> tt;

int probeTT(const uint64_t& key, int depth, int alpha, int beta, SearchInfo& si) {
	auto it = tt.find(key);
	if (it != tt.end()) {
		it->second.age = 0;
		if (it->second.depth < depth) return NO_VALUE;
		if (it->second.flag == TT_EXACT) {
			return it->second.score;
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

void storeTT(const uint64_t& key, int depth, int score, int flag) {
	auto it = tt.find(key);
	if (it != tt.end()) {
		it->second.depth = depth;
		it->second.score = score;
		it->second.flag = flag;
		it->second.age = 0;
	}
	else {
		tt.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(depth, score, flag));
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