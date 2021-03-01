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
	EvalInfo ei;

	ei.kingRings[WHITE] = kingRing[lsb(b.pieces[KING] & b.colors[WHITE])];
	ei.kingRings[BLACK] = kingRing[lsb(b.pieces[KING] & b.colors[BLACK])];

	ei.pawns[WHITE] = b.pieces[PAWN] & b.colors[WHITE];
	ei.pawns[BLACK] = b.pieces[PAWN] & b.colors[BLACK];

	ei.safeSquares[WHITE] = ~(((ei.pawns[BLACK] >> 7) & ~fileAMask) | ((ei.pawns[BLACK] >> 9) & ~fileHMask)) & b.colors[NO_COLOR];
	ei.safeSquares[BLACK] = ~(((ei.pawns[WHITE] << 7) & ~fileHMask) | ((ei.pawns[WHITE] << 9) & ~fileAMask)) & b.colors[NO_COLOR];
	ei.attackSquares[WHITE] = ~ei.safeSquares[BLACK];
	ei.attackSquares[BLACK] = ~ei.safeSquares[WHITE];

	// Step 1: Material
	// The value of pieces change slightly as the game progresses to reflect their changing importance (e.g. pawns are more important in the endgame)
	for (int i = PAWN; i <= QUEEN; ++i) {
		ei.mg += countBits(b.pieces[i] & b.colors[WHITE]) * pieceValues[i][MG];
		ei.eg += countBits(b.pieces[i] & b.colors[WHITE]) * pieceValues[i][EG];

		ei.mg -= countBits(b.pieces[i] & b.colors[BLACK]) * pieceValues[i][MG];
		ei.eg -= countBits(b.pieces[i] & b.colors[BLACK]) * pieceValues[i][EG];
	}

	// Step 2: Piece-square tables
	// A basic evaluation of the placement of pieces
	ei.mg += b.psqt[MG];
	ei.eg += b.psqt[EG];
	
	// Step 3: Evaluate pawns
	evaluatePawns(b, ei, WHITE);
	evaluatePawns(b, ei, BLACK);

	// Step 4: Evaluate knights, bishops, rooks and queens
	evaluateKnights(b, ei, WHITE);
	evaluateKnights(b, ei, BLACK);

	evaluateBishops(b, ei, WHITE);
	evaluateBishops(b, ei, BLACK);

	evaluateRooks(b, ei, WHITE);
	evaluateRooks(b, ei, BLACK);

	evaluateQueens(b, ei, WHITE);
	evaluateQueens(b, ei, BLACK);
	
	// Step 5: Evaluate kings
	evaluateKing(b, ei, WHITE);
	evaluateKing(b, ei, BLACK);

	// Step 6: Evaluate space
	if (phase >= spacePhaseLimit) {
		evaluateSpace(b, ei, WHITE);
		evaluateSpace(b, ei, BLACK);
	}

	// Step 7: Tempo bonus
	ei.mg += (b.turn == WHITE) ? tempoBonus : -tempoBonus;
	ei.eg += (b.turn == WHITE) ? tempoBonus : -tempoBonus;

	// Step 8: Evaluate threats
	evaluateThreats(b, ei, WHITE);
	evaluateThreats(b, ei, BLACK);

	int eval = taperedScore(ei.mg, ei.eg, phase);
	return (color == WHITE) ? eval : -eval;
}

void evaluatePawns(const Board& b, EvalInfo& ei, int color) {
	// Step 0: Probe pawn hash table
	// As pawn structures are often the same for similar positions, we check if we have evaluated this pawn structure before
	int hashEval = probePawnHash(ei.pawns[color], color);
	int mg = 0;
	int eg = 0;
	uint64_t pawns = ei.pawns[color];

	if (hashEval != NO_VALUE) {
		mg = hashEval;
	} else {
		// Step 1: Supported pawns
		// We give a bonus to pawns that defend each other
		mg += (countBits((pawns & ((color == WHITE) ? (pawns >> 7) & ~fileAMask : (pawns << 7) & ~fileHMask)) |
						(pawns & ((color == WHITE) ? (pawns >> 9) & ~fileHMask : (pawns << 9) & ~fileAMask)))) *
						supportedPawnBonus;

		// Step 2: Phalanx pawns
		// We give a bonus to pawns that are beside each other
		mg += (countBits((pawns & (pawns >> 1) & ~fileHMask) | (pawns & (pawns << 1) & ~fileAMask))) * phalanxPawnBonus;

		// Step 3: Doubled pawns
		// We give a penalty if there are more than one pawns on each file
		for (int i = 0; i < 8; ++i) {
			if (countBits(pawns & fileMasks[i]) > 1) mg -= doubledPawnPenalty;
		}

		storePawnHash(pawns, mg, color);
	}

	eg = mg; // Update eg score to mg score as the above evaluation terms are not phase-dependent

	while (pawns) {
		int sqr = popBit(pawns);

		// Step 4: Isolated pawns
		// We give a penalty if there are pawns that do not have friendly pawns in neighboring files that could support them
		int isolated = ((neighborFileMasks[sqr % 8] & ~fileMasks[sqr % 8] & b.pieces[PAWN] & b.colors[color]) == 0) * isolatedPawnPenalty;
		mg -= isolated;
		eg -= isolated;

		// Step 5: Passed pawn
		// We give a bonus to pawns that have passed enemy pawns
		// We reduce the bonus if the passed pawn is being blocked by a piece
		int passedRank = passed(b, sqr, ei.pawns[!color], color);
		bool blocked = (b.squares[sqr + (color == WHITE) * 8] == EMPTY);
		mg += blocked ? passedBonus[passedRank][MG] : passedBlockedBonus[passedRank][MG];
		eg += blocked ? passedBonus[passedRank][EG] : passedBlockedBonus[passedRank][EG];
	}

	ei.mg += (color == WHITE) ? mg : -mg;
	ei.eg += (color == WHITE) ? eg : -eg;
}

void evaluateKnights(const Board& b, EvalInfo& ei, int color) {
	int eval = 0;
	uint64_t knights = b.pieces[KNIGHT] & b.colors[color];
	while (knights) {
		uint64_t attacks = knightAttacks[popBit(knights)];
		// Mobility
		eval += knightMobility[countBits(attacks & ei.safeSquares[color])];

		// Populate attack bitmap
		ei.attackSquares[color] |= attacks;

		// King attacks
		bool kingAttacker = (countBits(attacks & ei.kingRings[!color]) > 0);
		ei.kingAttackCount[color] += kingAttacker * kingAttackWeight[KNIGHT];
		ei.kingAttackerCount[color] += kingAttacker;
	}
	
	ei.mg += (color == WHITE) ? eval : -eval;
	ei.eg += (color == WHITE) ? eval : -eval;
}

void evaluateBishops(const Board& b, EvalInfo& ei, int color) {
	int eval = 0;
	int count = 0;
	uint64_t bishops = b.pieces[BISHOP] & b.colors[color];
	const uint64_t occ = ~b.colors[NO_COLOR];
	while (bishops) {
		int sqr = popBit(bishops);
		uint64_t attacks = getBishopMagic(occ, sqr);
		attacks &= ~b.colors[b.turn];

		// Add to attack bitmap
		ei.attackSquares[color] |= attacks;

		// Bad bishops
		int bishopPawns = countBits(b.pieces[PAWN] & b.colors[color] & squareColorMasks[squareColor(sqr)]);
		int blockedCentrePawns = countBits((b.pieces[PAWN] >> 8) & b.pieces[PAWN] & middleFileMask);
		eval -= badBishopPenalty * (bishopPawns * (1 + blockedCentrePawns));

		// Mobility
		eval += bishopMobility[countBits(attacks & ei.safeSquares[color])];

		// King attacks
		bool kingAttacker = (countBits(attacks & ei.kingRings[!color]) > 0);
		ei.kingAttackCount[color] += kingAttacker * kingAttackWeight[BISHOP];
		ei.kingAttackerCount[color] += kingAttacker;

		count++;
	}
	// Bishop pair bonus
	if (count >= 2) eval += bishopPairBonus;
	
	ei.mg += (color == WHITE) ? eval : -eval;
	ei.eg += (color == WHITE) ? eval : -eval;
}

void evaluateRooks(const Board& b, EvalInfo& ei, int color) {
	int eval = 0;
	uint64_t rooks = b.pieces[ROOK] & b.colors[color];
	const uint64_t occ = ~b.colors[NO_COLOR];
	while (rooks) {
		int sqr = popBit(rooks);
		uint64_t attacks = getRookMagic(occ, sqr);
		attacks &= ~b.colors[b.turn];
		// Mobility
		eval += rookMobility[countBits(attacks & ei.safeSquares[color])];

		// Add to attack bitmap
		ei.attackSquares[color] |= attacks;

		// King attacks
		bool kingAttacker = (countBits(attacks & ei.kingRings[!color]) > 0);
		ei.kingAttackCount[color] += kingAttacker * kingAttackWeight[ROOK];
		ei.kingAttackerCount[color] += kingAttacker;

		// Semi-open and open file bonus
		eval += rookFileBonus[openFile(b, sqr % 8)];
	}

	ei.mg += (color == WHITE) ? eval : -eval;
	ei.eg += (color == WHITE) ? eval : -eval;
}

void evaluateQueens(const Board& b, EvalInfo& ei, int color) {
	int eval = 0;
	uint64_t queens = b.pieces[QUEEN] & b.colors[color];
	const uint64_t occ = ~b.colors[NO_COLOR];
	while (queens) {
		int sqr = popBit(queens);
		uint64_t attacks = (getBishopMagic(occ, sqr) | getRookMagic(occ, sqr)) & ~b.colors[b.turn];
		// Mobility
		eval += queenMobility[countBits(attacks & ei.safeSquares[color])];

		// Add to attack bitmap
		ei.attackSquares[color] |= attacks;

		// King attacks
		bool kingAttacker = (countBits(attacks & ei.kingRings[!color]) > 0);
		ei.kingAttackCount[color] += kingAttacker * kingAttackWeight[QUEEN];
		ei.kingAttackerCount[color] += kingAttacker;
	}
	ei.mg += (color == WHITE) ? eval : -eval;
	ei.eg += (color == WHITE) ? eval : -eval;
}

void evaluateKing(const Board& b, EvalInfo& ei, int color) {
	int mg = 0;
	int eg = 0;

	int kingSquare = lsb(b.pieces[KING] & b.colors[color]);

	// We give a penalty if our king ring is attacked by enemy pieces
	// The penalty follows a sigmoid curve with the number of attacking pieces
	mg -= ei.kingAttackCount[!color] * kingAttackPenalty * kingAttackerWeight[std::min(ei.kingAttackerCount[!color], 7)] / 24;


	uint64_t shelter = ei.kingRings[color] & rankMasks[kingSquare / 8 + ((color == WHITE) ? 1 : -1)] & ~(rank1Mask | rank8Mask) |
						(((1ull << kingSquare) >> 1) & ~fileHMask) | (((1ull << kingSquare) << 1) & ~fileAMask);
	mg += kingShelterBonus[countBits(shelter & b.pieces[PAWN] & b.colors[color])];

	// We penalise weak squares near king (king ring squares that are attacked but not defended by our pieces or pawns)
	// int weak = countBits(ei.attackSquares[!color] & ~ei.attackSquares[color] & ei.kingRings[color]) * weakSquarePenalty;
	// mg -= weak;
	// eg -= weak;

	// We give a penalty if the king is on a semi-open or open file
	mg -= kingFilePenalty[openFile(b, kingSquare % 8)];

	ei.mg += (color == WHITE) ? mg : -mg;
	ei.eg += (color == WHITE) ? eg : -eg;
}

void evaluateSpace(const Board& b, EvalInfo& ei, int color) {
	int pieceCount = countBits(b.colors[color] & ~b.pieces[PAWN] & ~b.pieces[KING]);
	uint64_t spaceArea = ei.safeSquares[color] & centerMasks[color] & b.colors[NO_COLOR];
	int space = countBits(spaceArea) * std::max(pieceCount - 3, 0);
	
	ei.mg += (color == WHITE) ? space : -space;
}

void evaluateThreats(const Board& b, EvalInfo& ei, int color) {
	int eval = 0;

	// Step 1: Safe pawn threat
	// We give a bonus for threats made by our safe pawns to non-pawn enemies
	// A safe pawn is a pawn that is not attacked by enemies or defended by us
	uint64_t safePawns = (ei.attackSquares[color] | ~ei.attackSquares[!color]) & b.pieces[PAWN] & b.colors[color];
	uint64_t safePawnAttackSquares = (color == WHITE) ? (((safePawns << 7) & ~fileHMask) | ((safePawns << 9) & ~fileAMask)) :
														(((safePawns >> 7) & ~fileAMask) | ((safePawns >> 9) & ~fileHMask));
	eval += safePawnThreatBonus * countBits(safePawnAttackSquares & b.colors[!color] & ~b.pieces[PAWN]);
	
	ei.mg += (color == WHITE) ? eval : -eval;
	ei.eg += (color == WHITE) ? eval : -eval;
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
	// Game phase is normalized between 24 (middlegame) and 0 (endgame)
	// We do not consider pawns in calculating game phase
	return std::min(countBits(b.pieces[KNIGHT] | b.pieces[BISHOP])
	           + 2 * countBits(b.pieces[ROOK])
			   + 4 * countBits(b.pieces[QUEEN]), 24);
}