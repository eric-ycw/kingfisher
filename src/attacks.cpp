#include "attacks.h"
#include "bitboard.h"
#include "board.h"
#include "types.h"

uint64_t pawnAttacks[SQUARE_NUM][2];
uint64_t knightAttacks[SQUARE_NUM];
uint64_t bishopAttacks[SQUARE_NUM][4];
uint64_t rookAttacks[SQUARE_NUM][4];
uint64_t queenAttacks[SQUARE_NUM][8];
uint64_t kingAttacks[SQUARE_NUM];

static void initPawnAttacks(const uint64_t& bb, int sqr) {
	if (bb & ~fileHMask) {
		setBitIfValid(pawnAttacks[sqr][WHITE], sqr + NE);
		setBitIfValid(pawnAttacks[sqr][BLACK], sqr + SE);
	}
	if (bb & ~fileAMask) {
		setBitIfValid(pawnAttacks[sqr][WHITE], sqr + NW);
		setBitIfValid(pawnAttacks[sqr][BLACK], sqr + SW);
	}
}

static void initKnightAttacks(const uint64_t& bb, int sqr) {
	if (bb & ~fileHMask) {
		setBitIfValid(knightAttacks[sqr], sqr + NNE);
		setBitIfValid(knightAttacks[sqr], sqr + SSE);
		if (bb & ~fileGMask) {
			setBitIfValid(knightAttacks[sqr], sqr + NEE);
			setBitIfValid(knightAttacks[sqr], sqr + SEE);
		}
	}
	if (bb & ~fileAMask) {
		setBitIfValid(knightAttacks[sqr], sqr + NNW);
		setBitIfValid(knightAttacks[sqr], sqr + SSW);
		if (bb & ~fileBMask) {
			setBitIfValid(knightAttacks[sqr], sqr + NWW);
			setBitIfValid(knightAttacks[sqr], sqr + SWW);
		}
	}
}

static void initBishopAttacks(int sqr) {
	int cur = sqr;
	for (int i = 0; i < 4; ++i) {
		while (validSquare(cur)) {
			if ((i % 2 == 0 && cur % 8 == 7) || (i % 2 == 1 && cur % 8 == 0)) break;
			cur += bishopDirs[i];
			setBitIfValid(bishopAttacks[sqr][i], cur);
		}
		cur = sqr;
	}
}

static void initRookAttacks(int sqr) {
	int cur = sqr;
	for (int i = 0; i < 4; ++i) {
		while (validSquare(cur)) {
			if ((i == 1 && cur % 8 == 7) || (i == 3 && cur % 8 == 0)) break;
			cur += rookDirs[i];
			setBitIfValid(rookAttacks[sqr][i], cur);
		}
		cur = sqr;
	}
}

static void initQueenAttacks(int sqr) {
	for (int i = 0; i < 8; ++i) {
		if (i < 4) {
			queenAttacks[sqr][i] = rookAttacks[sqr][i];
		}
		else {
			queenAttacks[sqr][i] = bishopAttacks[sqr][i - 4];
		}
	}
}

static void initKingAttacks(const uint64_t& bb, int sqr) {
	setBitIfValid(kingAttacks[sqr], sqr + N);
	setBitIfValid(kingAttacks[sqr], sqr + S);
	if (bb & ~fileHMask) {
		setBitIfValid(kingAttacks[sqr], sqr + E);
		setBitIfValid(kingAttacks[sqr], sqr + NE);
		setBitIfValid(kingAttacks[sqr], sqr + SE);
	}
	if (bb & ~fileAMask) {
		setBitIfValid(kingAttacks[sqr], sqr + W);
		setBitIfValid(kingAttacks[sqr], sqr + NW);
		setBitIfValid(kingAttacks[sqr], sqr + SW);
	}
}

void initAttacks() {
	for (int i = 0; i < SQUARE_NUM; ++i) {
		uint64_t bb = 1ull << i;
		initPawnAttacks(bb, i);
		initKnightAttacks(bb, i);
		initBishopAttacks(i);
		initRookAttacks(i);
		initQueenAttacks(i);  // Called after bishop and rook attacks are initialized
		initKingAttacks(bb, i);
	}
}

uint64_t getBishopAttacks(const Board& b, int color, int sqr) {
	uint64_t finalAttacks = 0ull;
	for (int i = 0; i < 4; ++i) {
		const uint64_t attacks = bishopAttacks[sqr][i];
		const uint64_t blockers = ~b.colors[NO_COLOR] & attacks;
		finalAttacks |= ((!blockers) ? attacks : (attacks ^ bishopAttacks[generalBitscan(blockers, bishopDirs[i])][i]) & ~b.colors[color]);
	}
	return finalAttacks;
}

uint64_t getBishopAttacks(const uint64_t& occupied, int sqr) {
	uint64_t finalAttacks = 0ull;
	for (int i = 0; i < 4; ++i) {
		const uint64_t attacks = bishopAttacks[sqr][i];
		const uint64_t blockers = occupied & attacks;
		finalAttacks |= ((!blockers) ? attacks : (attacks ^ bishopAttacks[generalBitscan(blockers, bishopDirs[i])][i]));
	}
	return finalAttacks;
}

uint64_t getRookAttacks(const Board& b, int color, int sqr) {
	uint64_t finalAttacks = 0ull;
	for (int i = 0; i < 4; ++i) {
		const uint64_t attacks = rookAttacks[sqr][i];
		const uint64_t blockers = ~b.colors[NO_COLOR] & attacks;
		finalAttacks |= ((!blockers) ? attacks : (attacks ^ rookAttacks[generalBitscan(blockers, rookDirs[i])][i]) & ~b.colors[color]);
	}
	return finalAttacks;
}

uint64_t getRookAttacks(const uint64_t& occupied, int sqr) {
	uint64_t finalAttacks = 0ull;
	for (int i = 0; i < 4; ++i) {
		const uint64_t attacks = rookAttacks[sqr][i];
		const uint64_t blockers = occupied & attacks;
		finalAttacks |= ((!blockers) ? attacks : (attacks ^ rookAttacks[generalBitscan(blockers, rookDirs[i])][i]));
	}
	return finalAttacks;
}

uint64_t getQueenAttacks(const Board& b, int color, int sqr) {
	uint64_t finalAttacks = 0ull;
	for (int i = 0; i < 8; ++i) {
		const uint64_t attacks = queenAttacks[sqr][i];
		const uint64_t blockers = ~b.colors[NO_COLOR] & attacks;
		finalAttacks |= ((!blockers) ? attacks : (attacks ^ queenAttacks[generalBitscan(blockers, queenDirs[i])][i]) & ~b.colors[color]);
	}
	return finalAttacks;
}

bool squareIsAttacked(const Board& b, int color, int sqr) {
	uint64_t enemy = b.colors[!color];
	return (pawnAttacks[sqr][color] & enemy & b.pieces[PAWN])
		|| (knightAttacks[sqr] & enemy & b.pieces[KNIGHT])
		|| (getBishopAttacks(b, color, sqr) & enemy & (b.pieces[BISHOP] | b.pieces[QUEEN]))
		|| (getRookAttacks(b, color, sqr) & enemy & (b.pieces[ROOK] | b.pieces[QUEEN]))
		|| (kingAttacks[sqr] & enemy & b.pieces[KING]);
}

uint64_t squareAttackers(const Board& b, int color, int sqr) {
	return ((pawnAttacks[sqr][color] & b.pieces[PAWN])
		 | (knightAttacks[sqr] & b.pieces[KNIGHT])
		 | (getBishopAttacks(b, color, sqr) & (b.pieces[BISHOP] | b.pieces[QUEEN]))
		 | (getRookAttacks(b, color, sqr) & (b.pieces[ROOK] | b.pieces[QUEEN]))
		 | (kingAttacks[sqr] & b.pieces[KING])) & b.colors[!color];
}

int smallestAttacker(const Board&b, const uint64_t& attackers) {
	for (int i = PAWN; i <= KING; ++i) {
		if (attackers & b.pieces[i]) return i;
	}
	return EMPTY;
}

bool inCheck(const Board& b, int color) {
	int kingsqr = lsb(b.pieces[KING] & b.colors[color]);
	return squareIsAttacked(b, color, kingsqr);
}