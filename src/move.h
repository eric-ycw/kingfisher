#ifndef MOVE_H
#define MOVE_H

#include "types.h"

enum MoveFlag { NORMAL_MOVE, CASTLE_MOVE, EP_MOVE, PROMOTION_KNIGHT, PROMOTION_BISHOP, PROMOTION_ROOK, PROMOTION_QUEEN };

static constexpr int NO_SCORE = INT_MIN + 1;

// A move code is 16 bits
// [ffffff][tttttt][gggg]
// f: 6 bits, from square
// t: 6 bits, to square
// g: 4 bits, move flag

struct ScoredMove {

	uint16_t m;
	int score = 0;

	ScoredMove() {}

	ScoredMove(const uint16_t& mParam) {
		m = mParam;
	}

	inline void operator=(const uint16_t& mParam) {
		m = mParam;
	}

	inline bool operator==(const uint16_t& mParam) const {
		return (m == mParam);
	}

	inline bool operator!=(const uint16_t& mParam) const {
		return (m != mParam);
	}
};

static uint16_t createMove(const int& from, const int& to, const uint8_t& flag) {
	return (from << 10) | (to << 4) | flag;
}

Undo makeMove(Board& b, const uint16_t& m);
Undo makeNormalMove(Board& b, const uint16_t& m);
Undo makeEnPassantMove(Board& b, const uint16_t& m);
Undo makePromotionMove(Board& b, const uint16_t& m);
Undo makeCastleMove(Board& b, const uint16_t& m);
void undoMove(Board& b, const uint16_t& m, const Undo& u);

Undo makeNullMove(Board& b);
void undoNullMove(Board& b, const Undo& u);

void updateCastleRights(Board& b, const uint16_t& m);

bool moveIsPsuedoLegal(const Board& b, const uint16_t& m);

#define moveFrom(move) (move >> 10)
#define moveTo(move) ((move & 0x3f0) >> 4)
#define moveFlag(move) (move & 0xf)

#endif
