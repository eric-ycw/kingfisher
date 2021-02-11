#include "attacks.h"
#include "bitboard.h"
#include "board.h"
#include "evaluate.h"
#include "tt.h"
#include "types.h"

int psqtScore(int piece, int sqr, float phase) {
	switch (piece) {
	case PAWN:
		return pawnPSQT[sqr];
	case KNIGHT:
		return knightPSQT[sqr];
	case BISHOP:
		return bishopPSQT[sqr];
	case ROOK:
		return rookPSQT[sqr];
	case QUEEN:
		return queenPSQT[sqr];
	case KING:
		return taperedScore(kingPSQT[sqr][MG], kingPSQT[sqr][EG], phase);
	}
}

int evaluate(const Board& b, int color) {
	float phase = getPhase(b);
	int eval = 0;

	// Piece values
	int whiteMaterial = 0;
	int blackMaterial = 0;
	for (int i = PAWN; i <= QUEEN; ++i) {
		whiteMaterial += countBits(b.pieces[i] & b.colors[WHITE]) * taperedScore(pieceValues[i][MG], pieceValues[i][EG], phase);
		blackMaterial += countBits(b.pieces[i] & b.colors[BLACK]) * taperedScore(pieceValues[i][MG], pieceValues[i][EG], phase);
	}
	eval += whiteMaterial - blackMaterial;

	// Piece-square table
	eval += b.psqt;

	// King piece-square table
	int whiteKingSqr = lsb(b.pieces[KING] & b.colors[WHITE]);
	int blackKingSqr = lsb(b.pieces[KING] & b.colors[BLACK]);
	eval += psqtScore(KING, psqtSquare(whiteKingSqr, WHITE), phase) - psqtScore(KING, psqtSquare(blackKingSqr, BLACK), phase);

	// Evaluate pawns
	uint64_t whitePawns = b.pieces[PAWN] & b.colors[WHITE];
	uint64_t blackPawns = b.pieces[PAWN] & b.colors[BLACK];
	eval += evaluatePawns(b, whitePawns, blackPawns, WHITE, phase);
	eval -= evaluatePawns(b, blackPawns, whitePawns, BLACK, phase);

	// Mobility info
	uint64_t whiteSafeSquares = ~(((blackPawns >> 7) & ~fileAMask) | ((blackPawns >> 9) & ~fileHMask)) & b.colors[NO_COLOR];
	uint64_t blackSafeSquares = ~(((whitePawns << 7) & ~fileHMask) | ((whitePawns << 9) & ~fileAMask)) & b.colors[NO_COLOR];

	// King attack info
	uint64_t whiteKingRing = genKingRing(whiteKingSqr);
	uint64_t blackKingRing = genKingRing(blackKingSqr);

	// Evaluate knights, bishops, rooks and queens
	eval += evaluateKnights(b, b.pieces[KNIGHT] & b.colors[WHITE], whiteSafeSquares, blackKingRing);
	eval += evaluateBishops(b, b.pieces[BISHOP] & b.colors[WHITE], whiteSafeSquares, blackKingRing, WHITE);
	eval += evaluateRooks(b, b.pieces[ROOK] & b.colors[WHITE], whiteSafeSquares, blackKingRing, WHITE);
	eval += evaluateQueens(b, b.pieces[QUEEN] & b.colors[WHITE], whiteSafeSquares, blackKingRing, WHITE);

	eval -= evaluateKnights(b, b.pieces[KNIGHT] & b.colors[BLACK], blackSafeSquares, whiteKingRing);
	eval -= evaluateBishops(b, b.pieces[BISHOP] & b.colors[BLACK], blackSafeSquares, whiteKingRing, BLACK);
	eval -= evaluateRooks(b, b.pieces[ROOK] & b.colors[BLACK], blackSafeSquares, whiteKingRing, BLACK);
	eval -= evaluateQueens(b, b.pieces[QUEEN] & b.colors[BLACK], blackSafeSquares, whiteKingRing, BLACK);

	return (color == WHITE) ? eval : -eval;
}

int evaluatePawns(const Board& b, uint64_t pawns, const uint64_t& enemyPawns, int color, float phase) {
	// Probe pawn hash table
	int eval = 0;
	int hashEval = probePawnHash(pawns, color);
	if (hashEval != NO_VALUE) {
		eval = hashEval;
	} else {
		// Supported pawns
		int supported = (countBits((pawns & ((color == WHITE) ? (pawns >> 7) & ~fileAMask : (pawns << 7) & ~fileHMask)) |
						(pawns & ((color == WHITE) ? (pawns >> 9) & ~fileHMask : (pawns << 9) & ~fileAMask)))) *
						supportedPawnBonus;

		// Phalanx pawns
		int phalanx = (countBits((pawns & (pawns >> 1) & ~fileHMask) | (pawns & (pawns << 1) & ~fileAMask))) * phalanxPawnBonus;

		// Doubled pawns
		int doubled = 0;
		for (int i = 0; i < 8; ++i) {
			if (countBits(pawns & fileMasks[i]) > 1) doubled -= doubledPawnPenalty;
		}

		eval = supported + phalanx - doubled;
		storePawnHash(pawns, eval, color);
	}

	while (pawns) {
		int sqr = popBit(pawns);
		// Passed pawn
		int passedRank = passed(b, sqr, enemyPawns, color);
		int baseScore = taperedScore(passedBonus[passedRank][MG], passedBonus[passedRank][EG], phase);
		eval += (b.squares[sqr + (color == WHITE) * 8] == EMPTY) ? baseScore : baseScore / passedBlockReduction;
	}

	return eval;
}

int evaluateKnights(const Board& b, uint64_t pieces, const uint64_t& safeSquares, const uint64_t& enemyKingRing) {
	int eval = 0;
	while (pieces) {
		uint64_t attacks = knightAttacks[popBit(pieces)];
		// Mobility
		eval += knightMobility[countBits(attacks & safeSquares)];
		// King attacks
		int kingAttacks = countBits(attacks & enemyKingRing);
		eval += kingAttacks * kingAttackBonus;
		eval += (kingAttacks > 0) * kingAttackerBonus[KNIGHT];
	}
	return eval;
}

int evaluateBishops(const Board& b, uint64_t pieces, const uint64_t& safeSquares, const uint64_t& enemyKingRing, int color) {
	int eval = 0;
	int count = 0;
	const uint64_t valid = ~b.colors[b.turn];
	while (pieces) {
		uint64_t attacks = getBishopAttacks(b, valid, color, popBit(pieces));
		// Mobility
		eval += bishopMobility[countBits(attacks & safeSquares)];
		// King attacks
		int kingAttacks = countBits(attacks & enemyKingRing);
		eval += kingAttacks * kingAttackBonus;
		eval += (kingAttacks > 0) * kingAttackerBonus[BISHOP];
		count++;
	}
	// Bishop pair bonus
	if (count >= 2) eval += bishopPairBonus;
	return eval;
}

int evaluateRooks(const Board& b, uint64_t pieces, const uint64_t& safeSquares, const uint64_t& enemyKingRing, int color) {
	int eval = 0;
	const uint64_t valid = ~b.colors[b.turn];
	while (pieces) {
		int sqr = popBit(pieces);
		uint64_t attacks = getRookAttacks(b, valid, color, sqr);
		// Mobility
		eval += rookMobility[countBits(attacks & safeSquares)];
		// King attacks
		int kingAttacks = countBits(attacks & enemyKingRing);
		eval += kingAttacks * kingAttackBonus;
		eval += (kingAttacks > 0) * kingAttackerBonus[ROOK];
		// Semi-open and open file bonus
		eval += rookFileBonus[openFile(b, sqr % 8)];
	}
	return eval;
}

int evaluateQueens(const Board& b, uint64_t pieces, const uint64_t& safeSquares, const uint64_t& enemyKingRing, int color) {
	int eval = 0;
	const uint64_t valid = ~b.colors[b.turn];
	while (pieces) {
		uint64_t attacks = getQueenAttacks(b, valid, color, popBit(pieces));
		// Mobility
		eval += queenMobility[countBits(attacks & safeSquares)];
		// King attacks
		int kingAttacks = countBits(attacks & enemyKingRing);
		eval += kingAttacks * kingAttackBonus;
		eval += (kingAttacks > 0) * kingAttackerBonus[QUEEN];
	}
	return eval;
}

int passed(const Board& b, int sqr, const uint64_t& enemyPawns, int color) {
	int rank = sqr / 8;
	int file = sqr % 8;
	uint64_t rankMask = 0ull;
	if (color == WHITE) {
		for (int i = 6; i > rank; --i) {
			rankMask |= rankMasks[i];
		}
	}
	else {
		for (int i = 1; i < rank; ++i) {
			rankMask |= rankMasks[i];
		}
	}
	uint64_t fileMask = fileMasks[file] | fileMasks[std::max(0, file - 1)] | fileMasks[std::min(7, file + 1)];
	return (!countBits(rankMask & fileMask & enemyPawns)) ? ((color == WHITE) ? rank : 7 - rank) : 0;
}

int openFile(const Board& b, int file) {
	uint64_t whitePawnsOnFile = fileMasks[file] & b.pieces[PAWN] & b.colors[WHITE];
	uint64_t blackPawnsOnFile = fileMasks[file] & b.pieces[PAWN] & b.colors[BLACK];
	return !whitePawnsOnFile + !blackPawnsOnFile;
}

uint64_t genKingRing(int sqr) {
	uint64_t kingSquare = (1ull << sqr);
	uint64_t diagonals = ((kingSquare >> 7) & ~fileAMask) | ((kingSquare >> 9) & ~fileHMask) | ((kingSquare << 7) & ~fileHMask) | ((kingSquare << 9) & ~fileAMask);
	uint64_t cardinals = ((kingSquare >> 1) & ~fileHMask) | ((kingSquare << 1) & ~fileAMask) | (kingSquare >> 8) | (kingSquare << 8);
	return kingSquare | diagonals | cardinals;
}

float getPhase(const Board& b) {
	int material = 0;
	for (int i = PAWN; i <= QUEEN; ++i) {
		material += pieceValues[i][MG] * countBits(b.pieces[i]);
	}
	return std::min((float)1.0, material / materialSum);
}