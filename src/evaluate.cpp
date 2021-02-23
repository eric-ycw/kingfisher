#include "attacks.h"
#include "bitboard.h"
#include "board.h"
#include "evaluate.h"
#include "masks.h"
#include "tt.h"
#include "types.h"

int psqtScore(int piece, int sqr, int phase) {
	switch (piece) {
	default:
		return 0;
	case PAWN:
		return pawnPSQT[sqr];
	case KNIGHT:
		return knightPSQT[phase][sqr];
	case BISHOP:
		return bishopPSQT[phase][sqr];
	case ROOK:
		return rookPSQT[phase][sqr];
	case QUEEN:
		return queenPSQT[phase][sqr];
	case KING:
		return kingPSQT[phase][sqr];
	}
}

int evaluate(const Board& b, int color) {
	int phase = getPhase(b);
	int eval = 0;

	// Step 1: Material
	// The value of pieces change slightly as the game progresses to reflect their changing importance (e.g. pawns are more important in the endgame)
	int whiteMaterial = 0;
	int blackMaterial = 0;
	for (int i = PAWN; i <= QUEEN; ++i) {
		whiteMaterial += countBits(b.pieces[i] & b.colors[WHITE]) * taperedScore(pieceValues[i][MG], pieceValues[i][EG], phase);
		blackMaterial += countBits(b.pieces[i] & b.colors[BLACK]) * taperedScore(pieceValues[i][MG], pieceValues[i][EG], phase);
	}
	eval += whiteMaterial - blackMaterial;

	// Step 2a: Non-king piece-square tables
	// A basic evaluation of the placement of pieces (e.g. knights are bad near corners)
	eval += taperedScore(b.psqt[MG], b.psqt[EG], phase);

	// Step 2b: King piece-square table
	// In the middlegame, the king is best placed near the corners of the board
	// In the endgame, the king is best placed in the middle of the board
	int whiteKingSqr = lsb(b.pieces[KING] & b.colors[WHITE]);
	int blackKingSqr = lsb(b.pieces[KING] & b.colors[BLACK]);

	// King attack info
	uint64_t whiteKingRing = kingRing[whiteKingSqr];
	uint64_t blackKingRing = kingRing[blackKingSqr];
	
	// Step 3: Evaluate pawns
	uint64_t whitePawns = b.pieces[PAWN] & b.colors[WHITE];
	uint64_t blackPawns = b.pieces[PAWN] & b.colors[BLACK];
	eval += evaluatePawns(b, whitePawns, blackPawns, blackKingRing, WHITE, phase);
	eval -= evaluatePawns(b, blackPawns, whitePawns, whiteKingRing, BLACK, phase);

	// Mobility info
	uint64_t whiteSafeSquares = ~(((blackPawns >> 7) & ~fileAMask) | ((blackPawns >> 9) & ~fileHMask)) & b.colors[NO_COLOR];
	uint64_t blackSafeSquares = ~(((whitePawns << 7) & ~fileHMask) | ((whitePawns << 9) & ~fileAMask)) & b.colors[NO_COLOR];

	// Attack info
	uint64_t whiteAttackSquares = ~blackSafeSquares;
	uint64_t blackAttackSquares = ~whiteSafeSquares;

	// Step 4: Evaluate knights, bishops, rooks and queens
	eval += evaluateKnights(b, b.pieces[KNIGHT] & b.colors[WHITE], whiteSafeSquares, blackKingRing, whiteAttackSquares);
	eval += evaluateBishops(b, b.pieces[BISHOP] & b.colors[WHITE], whiteSafeSquares, blackKingRing, whiteAttackSquares, WHITE);
	eval += evaluateRooks(b, b.pieces[ROOK] & b.colors[WHITE], whiteSafeSquares, blackKingRing, whiteAttackSquares, WHITE);
	eval += evaluateQueens(b, b.pieces[QUEEN] & b.colors[WHITE], whiteSafeSquares, blackKingRing, whiteAttackSquares, WHITE);

	eval -= evaluateKnights(b, b.pieces[KNIGHT] & b.colors[BLACK], blackSafeSquares, whiteKingRing, blackAttackSquares);
	eval -= evaluateBishops(b, b.pieces[BISHOP] & b.colors[BLACK], blackSafeSquares, whiteKingRing, blackAttackSquares, BLACK);
	eval -= evaluateRooks(b, b.pieces[ROOK] & b.colors[BLACK], blackSafeSquares, whiteKingRing, blackAttackSquares, BLACK);
	eval -= evaluateQueens(b, b.pieces[QUEEN] & b.colors[BLACK], blackSafeSquares, whiteKingRing, blackAttackSquares, BLACK);
	
	// Step 5: Evaluate kings
	eval += evaluateKing(b, whiteKingSqr, whiteKingRing, whiteAttackSquares, blackAttackSquares, WHITE, phase);
	eval -= evaluateKing(b, blackKingSqr, blackKingRing, blackAttackSquares, whiteAttackSquares, BLACK, phase);

	// Step 6: Evaluate space
	eval += evaluateSpace(b, whiteSafeSquares, WHITE, phase);
	eval -= evaluateSpace(b, blackSafeSquares, BLACK, phase);

	// Step 7: Tempo bonus
	eval += (b.turn == WHITE) ? tempoBonus : -tempoBonus;

	// Step 8: Evaluate threats
	eval += evaluateThreats(b, whiteAttackSquares, blackAttackSquares, WHITE);
	eval -= evaluateThreats(b, blackAttackSquares, whiteAttackSquares, BLACK);

	return (color == WHITE) ? eval : -eval;
}

int evaluateSpace(const Board& b, const uint64_t& safeSquares, const int& color, const int& phase) {
	if (phase < spacePhaseLimit) return 0;
	int pieceCount = countBits(b.colors[color] & ~b.pieces[PAWN] & ~b.pieces[KING]);
	uint64_t spaceArea = safeSquares & centerMasks[color] & b.colors[NO_COLOR];
	return countBits(spaceArea) * std::max(pieceCount - 3, 0);
}

int evaluateThreats(const Board&b, const uint64_t& defendedSquares, const uint64_t& attackedSquares, const int& color) {
	int eval = 0;

	// Step 1: Safe pawn threat
	// We give a bonus for threats made by our safe pawns to non-pawn enemies
	// A safe pawn is a pawn that is not attacked by enemies or defended by us
	uint64_t safePawns = (defendedSquares | ~attackedSquares) & b.pieces[PAWN] & b.colors[color];
	uint64_t safePawnAttackSquares = (color == WHITE) ? (((safePawns << 7) & ~fileHMask) | ((safePawns << 9) & ~fileAMask)) :
														(((safePawns >> 7) & ~fileAMask) | ((safePawns >> 9) & ~fileHMask));
	eval += safePawnThreatBonus * countBits(safePawnAttackSquares & b.colors[!color] & ~b.pieces[PAWN]);
	
	return eval;
	
}

int evaluatePawns(const Board& b, uint64_t pawns, const uint64_t& enemyPawns, const uint64_t& enemyKingRing, int color, int phase) {
	// Step 0: Probe pawn hash table
	// As pawn structures are often the same for similar positions, we check if we have evaluated this pawn structure before
	int eval = 0;
	int hashEval = probePawnHash(pawns, color);
	uint64_t pawnsCopy = pawns;

	if (hashEval != NO_VALUE) {
		eval = hashEval;
	} else {
		// Step 1: Supported pawns
		// We give a bonus to pawns that defend each other
		eval += (countBits((pawns & ((color == WHITE) ? (pawns >> 7) & ~fileAMask : (pawns << 7) & ~fileHMask)) |
						(pawns & ((color == WHITE) ? (pawns >> 9) & ~fileHMask : (pawns << 9) & ~fileAMask)))) *
						supportedPawnBonus;

		// Step 2: Phalanx pawns
		// We give a bonus to pawns that are beside each other
		eval += (countBits((pawns & (pawns >> 1) & ~fileHMask) | (pawns & (pawns << 1) & ~fileAMask))) * phalanxPawnBonus;

		// Step 3: Doubled pawns
		// We give a penalty if there are more than one pawns on each file
		for (int i = 0; i < 8; ++i) {
			if (countBits(pawns & fileMasks[i]) > 1) eval -= doubledPawnPenalty;
		}

		storePawnHash(pawns, eval, color);
	}

	while (pawns) {
		int sqr = popBit(pawns);

		// Step 4: Isolated pawns
		// We give a penalty if there are pawns that do not have friendly pawns in neighboring files that could support them
		eval -= ((neighborFileMasks[sqr % 8] & ~fileMasks[sqr % 8] & b.pieces[PAWN] & b.colors[color]) == 0) * isolatedPawnPenalty;

		// Step 5: Passed pawn
		// We give a bonus to pawns that have passed enemy pawns
		// We reduce the bonus if the passed pawn is being blocked by a piece
		int passedRank = passed(b, sqr, enemyPawns, color);
		bool blocked = (b.squares[sqr + (color == WHITE) * 8] == EMPTY);
		int mgPassedScore = blocked ? passedBonus[passedRank][MG] : passedBlockedBonus[passedRank][MG];
		int egPassedScore = blocked ? passedBonus[passedRank][EG] : passedBlockedBonus[passedRank][EG];
		eval += taperedScore(mgPassedScore, egPassedScore, phase);

		// King attacks
		eval += taperedScore(countBits(pawnAttacks[sqr][color] & enemyKingRing) * kingAttackerBonus[PAWN], 0, phase / 2);

	}

	return eval;
}

int evaluateKnights(const Board& b, uint64_t knights, const uint64_t& safeSquares, const uint64_t& enemyKingRing, uint64_t& attackSquares) {
	int eval = 0;
	while (knights) {
		uint64_t attacks = knightAttacks[popBit(knights)];
		// Mobility
		eval += knightMobility[countBits(attacks & safeSquares)];
		// Add to attack bitmap
		attackSquares |= attacks;
		// King attacks
		int kingAttacks = countBits(attacks & enemyKingRing);
		eval += kingAttacks * kingAttackBonus;
		eval += (kingAttacks > 0) * kingAttackerBonus[KNIGHT];
	}
	return eval;
}

int evaluateBishops(const Board& b, uint64_t bishops, const uint64_t& safeSquares, const uint64_t& enemyKingRing, uint64_t& attackSquares, int color) {
	int eval = 0;
	int count = 0;
	const uint64_t occ = ~b.colors[NO_COLOR];
	while (bishops) {
		int sqr = popBit(bishops);
		uint64_t attacks = *((((occ & bishopBlockerMasks[sqr]) * bishopMagics[sqr]) >> bishopMagicShifts[sqr]) + bishopMagicIndexIncrements[sqr]);
		attacks &= ~b.colors[b.turn];

		// Add to attack bitmap
		attackSquares |= attacks;

		// Bad bishops
		int bishopPawns = countBits(b.pieces[PAWN] & b.colors[color] & squareColorMasks[squareColor(sqr)]);
		int blockedCentrePawns = countBits((b.pieces[PAWN] >> 8) & b.pieces[PAWN] & middleFileMask);
		eval -= badBishopPenalty * (bishopPawns * (1 + blockedCentrePawns));

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

int evaluateRooks(const Board& b, uint64_t rooks, const uint64_t& safeSquares, const uint64_t& enemyKingRing, uint64_t& attackSquares, int color) {
	int eval = 0;
	const uint64_t occ = ~b.colors[NO_COLOR];
	while (rooks) {
		int sqr = popBit(rooks);
		uint64_t attacks = *((((occ & rookBlockerMasks[sqr]) * rookMagics[sqr]) >> rookMagicShifts[sqr]) + rookMagicIndexIncrements[sqr]);
		attacks &= ~b.colors[b.turn];
		// Mobility
		eval += rookMobility[countBits(attacks & safeSquares)];
		// Add to attack bitmap
		attackSquares |= attacks;
		// King attacks
		int kingAttacks = countBits(attacks & enemyKingRing);
		eval += kingAttacks * kingAttackBonus;
		eval += (kingAttacks > 0) * kingAttackerBonus[ROOK];
		// Semi-open and open file bonus
		eval += rookFileBonus[openFile(b, sqr % 8)];
	}
	return eval;
}

int evaluateQueens(const Board& b, uint64_t queens, const uint64_t& safeSquares, const uint64_t& enemyKingRing, uint64_t& attackSquares, int color) {
	int eval = 0;
	const uint64_t occ = ~b.colors[NO_COLOR];
	while (queens) {
		int sqr = popBit(queens);
		uint64_t diagonalAttacks = *((((occ & bishopBlockerMasks[sqr]) * bishopMagics[sqr]) >> bishopMagicShifts[sqr]) + bishopMagicIndexIncrements[sqr]);
		uint64_t cardinalAttacks = *((((occ & rookBlockerMasks[sqr]) * rookMagics[sqr]) >> rookMagicShifts[sqr]) + rookMagicIndexIncrements[sqr]);
		uint64_t attacks = (diagonalAttacks | cardinalAttacks) & ~b.colors[b.turn];
		// Mobility
		eval += queenMobility[countBits(attacks & safeSquares)];
		// Add to attack bitmap
		attackSquares |= attacks;
		// King attacks
		int kingAttacks = countBits(attacks & enemyKingRing);
		eval += kingAttacks * kingAttackBonus;
		eval += (kingAttacks > 0) * kingAttackerBonus[QUEEN];
	}
	return eval;
}

int evaluateKing(const Board& b, const int& sqr, const uint64_t& kingRing, const uint64_t& defendedSquares, const uint64_t& attackedSquares, int color, int phase) {
	int eval = 0;
	// We penalise weak squares near king (king ring squares that are attacked but not defended by our pieces or pawns)
	eval -= countBits(attackedSquares & ~defendedSquares & kingRing) * weakSquarePenalty;

	// We give a penalty if the king is on a semi-open or open file
	eval -= taperedScore(kingFilePenalty[openFile(b, sqr % 8)], 0, phase);

	return eval;
}

int passed(const Board& b, int sqr, const uint64_t& enemyPawns, int color) {
	int rank = sqr / 8;
	return (!countBits(passedPawnMasks[sqr][color] & enemyPawns)) ? ((color == WHITE) ? rank : 7 - rank) : 0;
}

bool isPassed(const Board&b, int sqr, int color) {
	return (passedPawnMasks[sqr][color] & b.pieces[PAWN] & b.colors[!color] == 0);
}

int openFile(const Board& b, int file) {
	uint64_t whitePawnsOnFile = fileMasks[file] & b.pieces[PAWN] & b.colors[WHITE];
	uint64_t blackPawnsOnFile = fileMasks[file] & b.pieces[PAWN] & b.colors[BLACK];
	return !whitePawnsOnFile + !blackPawnsOnFile;
}

int getPhase(const Board& b) {
	// Game phase is normalized between 256 (middlegame) and 0 (endgame)
	int material = 0;
	// We do not consider pawns in calculating game phase
	for (int i = KNIGHT; i <= QUEEN; ++i) {
		material += pieceValues[i][MG] * countBits(b.pieces[i]);
	}
	return std::min((material * 256 + (materialSum / 2)) / materialSum, 256);
}