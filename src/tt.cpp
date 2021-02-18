#include "board.h"
#include "move.h"
#include "search.h"
#include "tt.h"
#include "types.h"

TTInfo tt[TTMaxEntry];
PTTInfo ptt[TTMaxEntry];
qHashInfo qhash[qHashMaxEntry];
pHashInfo phash[pHashMaxEntry];

int probeTT(const uint64_t& key, const int& depth, const int& alpha, const int& beta, const int& ply, SearchInfo& si, int& ttEval) {
	
	// We have 4 buckets per index
	// Rightmost 32 bits of hash are used to get index
	// Leftmost 32 bits of hash are used for verification

	int index = (uint32_t)key & TTMaxEntry;
	for (int i = 0; i < 4; ++i) {
		auto& entry = tt[index + i];
		if (entry.key == (key >> 32)) {
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
	}

	return NO_VALUE;
}

Move probeHashMove(const uint64_t& key) {
	int index = (uint32_t)key & TTMaxEntry;
	for (int i = 0; i < 4; ++i) {
		auto& entry = tt[index + i];
		if (entry.key == (key >> 32)) {
			return entry.move;
		}
	}
	return NO_MOVE;
}

void storeTT(const uint64_t& key, const int& depth, const int& score, const int& flag, const int& eval, const int& ply, const Move& m) {
	int index = (uint32_t)key & TTMaxEntry;

	bool stored = false;

	for (int i = 0; i < 4; ++i) {
		auto& entry = tt[index + i];
		// Always replace if entry is empty or same position
		if (entry.key == 0 || entry.key == (key >> 32)) {
			entry.key = (key >> 32);
			entry.depth = depth;
			entry.score = score;
			entry.flag = flag;
			if (eval != NO_VALUE) entry.eval = eval;
			entry.ply = ply;
			entry.move = m;
			entry.age = 0;
			stored = true;
		}
	}
	
	// Buckets are full, we replace a random one
	// FIXME: Figure out a better replacement strategy
	if (!stored) {
		auto& entry = tt[index + key & 3];
		entry.key = (key >> 32);
		entry.depth = depth;
		entry.score = score;
		entry.flag = flag;
		if (eval != NO_VALUE) entry.eval = eval;
		entry.ply = ply;
		entry.move = m;
		entry.age = 0;
	}
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

void storeQHash(const uint64_t& key, const int& eval) {
	auto& entry = qhash[key & qHashMaxEntry];
	// Always replace
	entry.key = key;
	entry.eval = eval;
}

int probePawnHash(const uint64_t& key, const int& color) {
	auto& entry = phash[key & pHashMaxEntry];
	return (entry.key == key && entry.color == color) ? entry.staticEval : NO_VALUE;
}

void storePawnHash(const uint64_t& key, const int& staticEval, const int& color) {
	auto& entry = phash[key & pHashMaxEntry];
	// Always replace
	entry.key = key;
	entry.staticEval = staticEval;
	entry.color = color;
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