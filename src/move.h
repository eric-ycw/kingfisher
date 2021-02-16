#ifndef MOVE_H
#define MOVE_H

#include "types.h"

enum MoveFlag { NORMAL_MOVE, CASTLE_MOVE, EP_MOVE, PROMOTION_KNIGHT, PROMOTION_BISHOP, PROMOTION_ROOK, PROMOTION_QUEEN };

static constexpr int NO_SCORE = INT_MIN + 1;

struct Move {
	// [ffffff][tttttt][gggg]
	// f: 6 bits, from square
	// t: 6 bits, to square
	// g: 4 bits, move flag

	Move() {}

	Move(int fromParam, int toParam, uint8_t flagParam) {
		code = (fromParam << 10) | (toParam << 4) | flagParam;
		score = NO_SCORE;
	}

	Move(int fromParam, int toParam, uint8_t flagParam, int scoreParam) {
		code = (fromParam << 10) | (toParam << 4) | flagParam;
		score = scoreParam;
	}

	uint16_t code = 0;
	int score = NO_SCORE;

	inline int getFrom() const {
		return code >> 10;
	}

	inline int getTo() const {
		return (code & 0x3f0) >> 4;
	}

	inline int getFlag() const {
		return code & 0xf;
	}

	inline void operator=(const Move& m) {
		code = m.code;
		score = m.score;
	}

	inline bool operator==(const Move& m) const {
		return (code == m.code);
	}

	inline bool operator!=(const Move& m) const {
		return (code != m.code);
	}
};

static const Move NO_MOVE;

Undo makeMove(Board& b, const Move& m);
Undo makeNormalMove(Board& b, const Move& m);
Undo makeEnPassantMove(Board& b, const Move& m);
Undo makePromotionMove(Board& b, const Move& m);
Undo makeCastleMove(Board& b, const Move& m);
void undoMove(Board& b, const Move& m, const Undo& u);

Undo makeNullMove(Board& b);
void undoNullMove(Board& b, const Undo& u);

void updateCastleRights(Board& b, const Move& m);

bool moveIsPsuedoLegal(const Board& b, const Move& m);

#endif
