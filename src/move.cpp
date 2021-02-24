#include "attacks.h"
#include "bitboard.h"
#include "board.h"
#include "evaluate.h"
#include "move.h"
#include "types.h"

Undo makeMove(Board& b, const uint16_t& m) {
	if (!m) return Undo();
	assert(validSquare(moveFrom(m)) && validSquare(moveTo(m)));
	Undo u;
	switch (moveFlag(m)) {
	default:  // Promotion
		u = makePromotionMove(b, m);
		break;
	case NORMAL_MOVE:
		u = makeNormalMove(b, m);
		break;
	case EP_MOVE:
		u = makeEnPassantMove(b, m);
		break;
	case CASTLE_MOVE:
		u = makeCastleMove(b, m);
		break;
	}
	b.turn = !b.turn;
	if (b.epSquare == u.epSquare) b.epSquare = -1;
	b.history[++b.moveNum] = b.key;
	return u;
}

Undo makeNormalMove(Board& b, const uint16_t& m) {
	const int fromPiece = b.squares[moveFrom(m)];
	const int toPiece = b.squares[moveTo(m)];

	assert(fromPiece != EMPTY);

	const int fromType = pieceType(fromPiece);
	const int toType = pieceType(toPiece);	

	// Save board state in undo object
	Undo u(b.key, b.epSquare, b.castlingRights, b.fiftyMove, b.psqt[MG], b.psqt[EG], toPiece);

	b.fiftyMove = (fromType == PAWN || toPiece != EMPTY) ? 0 : b.fiftyMove + 1;

	b.squares[moveFrom(m)] = EMPTY;
	b.squares[moveTo(m)] = fromPiece;

	b.pieces[fromType] ^= (1ull << moveFrom(m)) ^ (1ull << moveTo(m));
	b.colors[b.turn] ^= (1ull << moveFrom(m)) ^ (1ull << moveTo(m));
	if (toPiece != EMPTY) {
		b.pieces[toType] ^= (1ull << moveTo(m));
		b.colors[!b.turn] ^= (1ull << moveTo(m));
	}
	b.colors[NO_COLOR] = ~(b.colors[b.turn] | b.colors[!b.turn]);

	b.key ^= pieceKeys[fromPiece][moveFrom(m)] ^ pieceKeys[fromPiece][moveTo(m)];
	if (toPiece != EMPTY) b.key ^= pieceKeys[toPiece][moveTo(m)];
	b.key ^= turnKey;

	// Update en passant square
	if (fromType == PAWN && (moveFrom(m) ^ moveTo(m)) == 16) {
		if (b.pieces[PAWN] & b.colors[!b.turn] & ((b.turn) ? rank5Mask : rank4Mask)) {
			b.epSquare = (b.turn) ? moveFrom(m) + S : moveFrom(m) + N;
			b.key ^= epKeys[b.epSquare % 8];
		}
	}
	if (b.epSquare == u.epSquare) b.epSquare = -1;

	// Update piece-square tables
	int mgPSQT = psqtScore(fromType, psqtSquare(moveTo(m), b.turn), MG) - psqtScore(fromType, psqtSquare(moveFrom(m), b.turn), MG);
	if (toPiece != EMPTY) mgPSQT += psqtScore(toType, psqtSquare(moveTo(m), !b.turn), MG);
	b.psqt[MG] += (b.turn == WHITE) ? mgPSQT : -mgPSQT;

	int egPSQT = psqtScore(fromType, psqtSquare(moveTo(m), b.turn), EG) - psqtScore(fromType, psqtSquare(moveFrom(m), b.turn), EG);
	if (toPiece != EMPTY) egPSQT += psqtScore(toType, psqtSquare(moveTo(m), !b.turn), EG);
	b.psqt[EG] += (b.turn == WHITE) ? egPSQT : -egPSQT;


	updateCastleRights(b, m);

	return u;
}

Undo makeEnPassantMove(Board& b, const uint16_t& m) {
	const int fromPiece = b.squares[moveFrom(m)];
	const int epPiece = (b.turn) ? W_PAWN : B_PAWN;
	const int epSquare = b.epSquare - 8 + (b.turn << 4);

	assert(pieceType(fromPiece) == PAWN);
	assert(b.squares[moveTo(m)] == EMPTY);
	assert(b.squares[epSquare] == epPiece);

	Undo u(b.key, b.epSquare, b.castlingRights, b.fiftyMove, b.psqt[MG], b.psqt[EG], epPiece);

	b.fiftyMove = 0;

	b.squares[moveFrom(m)] = EMPTY;
	b.squares[moveTo(m)] = fromPiece;
	b.squares[epSquare] = EMPTY;

	b.pieces[PAWN] ^= (1ull << moveFrom(m)) ^ (1ull << moveTo(m));
	b.colors[b.turn] ^= (1ull << moveFrom(m)) ^ (1ull << moveTo(m));

	b.pieces[PAWN] ^= (1ull << epSquare);
	b.colors[!b.turn] ^= (1ull << epSquare);

	b.colors[NO_COLOR] = ~(b.colors[b.turn] | b.colors[!b.turn]);

	b.key ^= pieceKeys[fromPiece][moveFrom(m)] ^ pieceKeys[fromPiece][moveTo(m)] ^ pieceKeys[epPiece][epSquare];
	b.key ^= turnKey;

	int mgPSQT = psqtScore(PAWN, psqtSquare(moveTo(m), b.turn), MG) - psqtScore(PAWN, psqtSquare(moveFrom(m), b.turn), MG) + psqtScore(PAWN, psqtSquare(epSquare, !b.turn), MG);
	b.psqt[MG] += (b.turn == WHITE) ? mgPSQT : -mgPSQT;

	int egPSQT = psqtScore(PAWN, psqtSquare(moveTo(m), b.turn), EG) - psqtScore(PAWN, psqtSquare(moveFrom(m), b.turn), EG) + psqtScore(PAWN, psqtSquare(epSquare, !b.turn), EG);
	b.psqt[EG] += (b.turn == WHITE) ? egPSQT : -egPSQT;

	return u;
}

Undo makePromotionMove(Board& b, const uint16_t& m) {
	const int fromPiece = b.squares[moveFrom(m)];
	const int toPiece = b.squares[moveTo(m)];

	const int fromType = pieceType(fromPiece);
	const int toType = pieceType(toPiece);

	int promotionPiece = EMPTY;
	switch (moveFlag(m)) {
	case PROMOTION_KNIGHT:
		promotionPiece = makePiece(KNIGHT, b.turn);
		break;
	case PROMOTION_BISHOP:
		promotionPiece = makePiece(BISHOP, b.turn);
		break;
	case PROMOTION_ROOK:
		promotionPiece = makePiece(ROOK, b.turn);
		break;
	case PROMOTION_QUEEN:
		promotionPiece = makePiece(QUEEN, b.turn);
		break;
	}

	assert(fromType == PAWN);
	assert(promotionPiece != EMPTY);

	// Save board state in undo object
	Undo u(b.key, b.epSquare, b.castlingRights, b.fiftyMove, b.psqt[MG], b.psqt[EG], toPiece);

	b.fiftyMove = 0;

	b.squares[moveFrom(m)] = EMPTY;
	b.squares[moveTo(m)] = promotionPiece;

	b.pieces[fromType] ^= (1ull << moveFrom(m));
	b.pieces[pieceType(promotionPiece)] ^= (1ull << moveTo(m));
	b.colors[b.turn] ^= (1ull << moveFrom(m)) ^ (1ull << moveTo(m));
	if (toPiece != EMPTY) {
		b.pieces[toType] ^= (1ull << moveTo(m));
		b.colors[!b.turn] ^= (1ull << moveTo(m));
	}
	b.colors[NO_COLOR] = ~(b.colors[b.turn] | b.colors[!b.turn]);

	b.key ^= pieceKeys[fromPiece][moveFrom(m)] ^ pieceKeys[promotionPiece][moveTo(m)];
	if (toPiece != EMPTY) b.key ^= pieceKeys[toPiece][moveTo(m)];
	b.key ^= turnKey;

	int mgPSQT = psqtScore(pieceType(promotionPiece), psqtSquare(moveTo(m), b.turn), MG) - psqtScore(PAWN, psqtSquare(moveFrom(m), b.turn), MG);
	if (toPiece != EMPTY) mgPSQT += psqtScore(toType, psqtSquare(moveTo(m), !b.turn), MG);
	b.psqt[MG] += (b.turn == WHITE) ? mgPSQT : -mgPSQT;

	int egPSQT = psqtScore(pieceType(promotionPiece), psqtSquare(moveTo(m), b.turn), EG) - psqtScore(PAWN, psqtSquare(moveFrom(m), b.turn), EG);
	if (toPiece != EMPTY) egPSQT += psqtScore(toType, psqtSquare(moveTo(m), !b.turn), EG);
	b.psqt[EG] += (b.turn == WHITE) ? egPSQT : -egPSQT;

	return u;
}

Undo makeCastleMove(Board& b, const uint16_t& m) {
	const int fromPiece = b.squares[moveFrom(m)];
	const int fromType = pieceType(fromPiece);

	const int fromRook = moveFrom(m) + ((moveTo(m) == G1 || moveTo(m) == G8) ? 3 : -4);
	const int toRook = moveFrom(m) + ((moveTo(m) == G1 || moveTo(m) == G8) ? 1 : -1);

	const int rookPiece = makePiece(ROOK, b.turn);

	assert(fromType == KING);

	// Save board state in undo object
	Undo u(b.key, b.epSquare, b.castlingRights, b.fiftyMove, b.psqt[MG], b.psqt[EG], EMPTY);

	b.fiftyMove += 1;

	b.squares[moveFrom(m)] = EMPTY;
	b.squares[moveTo(m)] = fromPiece;
	b.squares[fromRook] = EMPTY;
	b.squares[toRook] = rookPiece;

	b.pieces[KING] ^= (1ull << moveFrom(m)) ^ (1ull << moveTo(m));
	b.pieces[ROOK] ^= (1ull << fromRook) ^ (1ull << toRook);
	b.colors[b.turn] ^= (1ull << moveFrom(m)) ^ (1ull << moveTo(m)) ^ (1ull << fromRook) ^ (1ull << toRook);

	b.colors[NO_COLOR] = ~(b.colors[b.turn] | b.colors[!b.turn]);

	b.key ^= pieceKeys[fromPiece][moveFrom(m)] ^ pieceKeys[fromPiece][moveTo(m)] ^ pieceKeys[rookPiece][fromRook] ^ pieceKeys[rookPiece][toRook];
	b.key ^= turnKey;

	updateCastleRights(b, m);

	int mgPSQT = psqtScore(ROOK, psqtSquare(toRook, b.turn), MG) - psqtScore(ROOK, psqtSquare(fromRook, b.turn), MG);
	b.psqt[MG] += (b.turn == WHITE) ? mgPSQT : -mgPSQT;

	int egPSQT = psqtScore(ROOK, psqtSquare(toRook, b.turn), EG) - psqtScore(ROOK, psqtSquare(fromRook, b.turn), EG);
	b.psqt[EG] += (b.turn == WHITE) ? egPSQT : -egPSQT;

	return u;
}

void undoMove(Board& b, const uint16_t& m, const Undo& u) {
	b.turn = !b.turn;
	b.key = u.key;
	b.epSquare = u.epSquare;
	b.castlingRights = u.castlingRights;
	b.psqt[MG] = u.psqt[MG];
	b.psqt[EG] = u.psqt[EG];
	b.fiftyMove = u.fiftyMove;
	b.history[b.moveNum--] = 0ull;

	assert(validSquare(moveFrom(m)) && validSquare(moveTo(m)));

	if (moveFlag(m) == EP_MOVE) {
		const int epSquare = b.epSquare - 8 + (b.turn << 4);

		b.squares[moveFrom(m)] = b.squares[moveTo(m)];
		b.squares[moveTo(m)] = EMPTY;
		b.squares[epSquare] = u.capturedPiece;

		b.pieces[PAWN] ^= (1ull << moveFrom(m)) ^ (1ull << moveTo(m));
		b.colors[b.turn] ^= (1ull << moveFrom(m)) ^ (1ull << moveTo(m));

		b.pieces[PAWN] ^= (1ull << epSquare);
		b.colors[!b.turn] ^= (1ull << epSquare);
	}
	else if (moveFlag(m) >= PROMOTION_KNIGHT) {
		const int capturedType = pieceType(u.capturedPiece);

		b.squares[moveFrom(m)] = makePiece(PAWN, b.turn);
		b.squares[moveTo(m)] = u.capturedPiece;

		int promotionPiece = makePiece(moveFlag(m) - 2, b.turn);

		b.pieces[PAWN] ^= (1ull << moveFrom(m));
		b.pieces[pieceType(promotionPiece)] ^= (1ull << moveTo(m));
		b.colors[b.turn] ^= (1ull << moveFrom(m)) ^ (1ull << moveTo(m));
		if (u.capturedPiece != EMPTY) {
			b.pieces[capturedType] ^= (1ull << moveTo(m));
			b.colors[!b.turn] ^= (1ull << moveTo(m));
		}
	}
	else if (moveFlag(m) == CASTLE_MOVE) {
		const int fromRook = moveFrom(m) + ((moveTo(m) == G1 || moveTo(m) == G8) ? 3 : -4);
		const int toRook = moveFrom(m) + ((moveTo(m) == G1 || moveTo(m) == G8) ? 1 : -1);

		b.squares[moveFrom(m)] = b.squares[moveTo(m)];
		b.squares[moveTo(m)] = EMPTY;
		b.squares[fromRook] = b.squares[toRook];
		b.squares[toRook] = EMPTY;

		b.pieces[KING] ^= (1ull << moveFrom(m)) ^ (1ull << moveTo(m));
		b.pieces[ROOK] ^= (1ull << fromRook) ^ (1ull << toRook);
		b.colors[b.turn] ^= (1ull << moveFrom(m)) ^ (1ull << moveTo(m)) ^ (1ull << fromRook) ^ (1ull << toRook);
	}
	else {  // NORMAL_MOVE
		b.squares[moveFrom(m)] = b.squares[moveTo(m)];
		b.squares[moveTo(m)] = u.capturedPiece;

		const int fromType = pieceType(b.squares[moveFrom(m)]);
		const int toType = pieceType(b.squares[moveTo(m)]);

		b.pieces[fromType] ^= (1ull << moveFrom(m)) ^ (1ull << moveTo(m));
		b.colors[b.turn] ^= (1ull << moveFrom(m)) ^ (1ull << moveTo(m));
		if (u.capturedPiece != EMPTY) {
			b.pieces[toType] ^= (1ull << moveTo(m));
			b.colors[!b.turn] ^= (1ull << moveTo(m));
		}
	}

	b.colors[NO_COLOR] = ~(b.colors[b.turn] | b.colors[!b.turn]);
}

Undo makeNullMove(Board& b) {
	Undo u(b.key, b.epSquare, b.castlingRights, b.fiftyMove, b.psqt[MG], b.psqt[EG], EMPTY);
	b.turn = !b.turn;
	b.key ^= turnKey;
	if (b.epSquare != -1) b.key ^= epKeys[b.epSquare % 8];
	b.epSquare = -1;
	return u;
}

void undoNullMove(Board& b, const Undo& u) {
	b.turn = !b.turn;
	b.key = u.key;
	b.epSquare = u.epSquare;
}

void updateCastleRights(Board& b, const uint16_t& m) {
	if (b.castlingRights & WK_CASTLING) {
		if ((b.squares[moveFrom(m)] == W_KING) ||
			(moveTo(m) == E1 || moveTo(m) == H1) ||
			(moveFrom(m) == H1 && b.squares[H1] == W_ROOK))
		{
			b.castlingRights ^= WK_CASTLING;
		}
	}
	if (b.castlingRights & WQ_CASTLING) {
		if ((b.squares[moveFrom(m)] == W_KING) ||
			(moveTo(m) == E1 || moveTo(m) == A1) ||
			(moveFrom(m) == A1 && b.squares[A1] == W_ROOK))
		{
			b.castlingRights ^= WQ_CASTLING;
		}
	}
	if (b.castlingRights & BK_CASTLING) {
		if ((b.squares[moveFrom(m)] == B_KING) ||
			(moveTo(m) == E8 || moveTo(m) == H8) ||
			(moveFrom(m) == H8 && b.squares[H8] == B_ROOK))
		{
			b.castlingRights ^= BK_CASTLING;
		}
	}
	if (b.castlingRights & BQ_CASTLING) {
		if ((b.squares[moveFrom(m)] == B_KING) ||
			(moveTo(m) == E8 || moveTo(m) == A8) ||
			(moveFrom(m) == A8 && b.squares[A8] == B_ROOK))
		{
			b.castlingRights ^= BQ_CASTLING;
		}
	}
}

bool moveIsPsuedoLegal(const Board& b, const uint16_t& m) {
	// Null move
	if (!m) return false;

	const int from = moveFrom(m);
	const int to = moveTo(m);
	const int flag = moveFlag(m);

	// Not a valid square, not moving
	if (!validSquare(from) || !validSquare(to) || from == to) return false;

	const int fromPiece = b.squares[from];
	const int toPiece = b.squares[to];

	// No piece to move
	if (fromPiece == EMPTY) return false;

	const int fromType = pieceType(fromPiece);
	const int toType = pieceType(toPiece);
	const int fromColor = pieceColor(fromPiece);
	const int toColor = pieceColor(toPiece);

	// Not our piece, en passant/promotion but not a pawn move, castling but not a king move, capturing a king
	if (fromColor != b.turn || (flag >= EP_MOVE && fromType != PAWN) || (flag == CASTLE_MOVE && fromType != KING) || (toType == KING)) return false;

	uint64_t us = b.colors[fromColor];
	uint64_t them = b.colors[!fromColor];
	uint64_t empty = b.colors[NO_COLOR];

	// For knights, bishops, queens, and rooks, moves are valid as long they can attack the square they're moving to
	switch (fromType) {
		case KNIGHT: {
			return checkBit(knightAttacks[from] & ~us, to);
			break;
		}

		case BISHOP: {
			return checkBit(getBishopAttacks(b, ~us, b.turn, from), to);
			break;
		}

		case ROOK: {
			return checkBit(getRookAttacks(b, ~us, b.turn, from), to);
			break;
		}

		case QUEEN: {
			return checkBit(getQueenAttacks(b, ~us, b.turn, from), to);
			break;
		}
	}

	assert(fromType == PAWN || fromType == KING);

	if (fromType == PAWN) {
		const int forward = (b.turn == WHITE) ? N : S;

		uint64_t attacks = pawnAttacks[from][fromColor];

		// Check for en passant
		if (flag == EP_MOVE) {
			return (to == b.epSquare) && checkBit(attacks & empty, to);
		}

		// Check for pawn pushes
		uint64_t pawnPushes = (((1ull << from) << 8) >> (b.turn << 4)) & empty;
		if (to == from + forward) {
			return checkBit(pawnPushes, to);
		}

		// Check for pawn double pushes
		uint64_t srank = (b.turn == WHITE) ? rank2Mask : rank7Mask;
		if ((to == from + forward * 2) && checkBit(srank, from)) {
			uint64_t pawnDoublePushes = ((pawnPushes << 8) >> (b.turn << 4)) & ((b.turn == WHITE) ? rank4Mask : rank5Mask) & empty;
			return checkBit(pawnDoublePushes, to);
		}

		// Promotion but not on the correct rank
		uint64_t prank = (b.turn == WHITE) ? ~rank8Mask : ~rank1Mask;
		if (flag >= PROMOTION_KNIGHT && checkBit(prank, to)) return false;

		// Check for captures
		return checkBit(attacks & them, to);
	}
	
	assert(fromType == KING);

	// Moving the king normally
	if (flag == NORMAL_MOVE) return checkBit(kingAttacks[from] & ~us, to);

	assert(flag == CASTLE_MOVE);

	// Castling
	switch (to) {

		default: return false;

		// White short castling
		case G1: {
			if (fromColor != WHITE || from != E1) return false;
			if ((b.castlingRights & WK_CASTLING) == 0) return false;

			if (b.squares[F1] != EMPTY || b.squares[G1] != EMPTY || b.squares[H1] != W_ROOK) return false;
			
			return true;
		}

		// White long castling
		case C1: {
			if (fromColor != WHITE || from != E1) return false;
			if ((b.castlingRights & WQ_CASTLING) == 0) return false;

			if (b.squares[D1] != EMPTY || b.squares[C1] != EMPTY || b.squares[B1] != EMPTY || b.squares[A1] != W_ROOK) return false;
			
			return true;
		}

		// Black short castling
		case G8: {
			if (fromColor != BLACK || from != E8) return false;
			if ((b.castlingRights & BK_CASTLING) == 0) return false;

			if (b.squares[F8] != EMPTY || b.squares[G8] != EMPTY || b.squares[H8] != B_ROOK) return false;

			return true;
		}

		// Black long castling
		case C8: {
			if (fromColor != BLACK || from != E8) return false;
			if ((b.castlingRights & BQ_CASTLING) == 0) return false;
			
			if (b.squares[D8] != EMPTY || b.squares[C8] != EMPTY || b.squares[B8] != EMPTY || b.squares[A8] != B_ROOK) return false;
			
			return true;
		}

	}

	return false;
}