#ifndef MOVE_H
#define MOVE_H

#include "types.h"

enum MoveFlag { NORMAL_MOVE, CASTLE_MOVE, EP_MOVE, PROMOTION_KNIGHT, PROMOTION_BISHOP, PROMOTION_ROOK, PROMOTION_QUEEN };

struct Move {
	// [ffffff][tttttt][gggg]
	// f -> 6 bits, from square
	// t -> 6 bits, to square
	// g -> 4 bits, move flag
	
	Move() {
		from = -1;
		to = -1;
		flag = -1;
		score = 0;
	}

	Move(int fromParam, int toParam, uint8_t flagParam) {
		from = fromParam;
		to = toParam;
		flag = flagParam;
		score = 0;
	}

	Move(int fromParam, int toParam, uint8_t flagParam, int scoreParam) {
		from = fromParam;
		to = toParam;
		flag = flagParam;
		score = scoreParam;
	}

	// uint16_t code;
	// int score;

 	int from;
	int to;
	uint8_t flag;
	int score;

	void operator=(const Move& m) {
		from = m.from;
		to = m.to;
		flag = m.flag;
		score = m.score;
	}

	bool operator==(const Move& m) const {
		return (from == m.from && to == m.to && flag == m.flag);
	}

	bool operator!=(const Move& m) const {
		return (from != m.from || to != m.to || flag != m.flag);
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

#endif
