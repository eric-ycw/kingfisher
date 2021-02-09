#include "attacks.h"
#include "bitboard.h"
#include "board.h"
#include "evaluate.h"
#include "move.h"
#include "types.h"

Undo makeMove(Board& b, const Move& m) {
	if (m == NO_MOVE) return Undo();
	assert(validSquare(m.getFrom()) && validSquare(m.getTo()));
	Undo u;
	switch (m.getFlag()) {
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

Undo makeNormalMove(Board& b, const Move& m) {
	const int fromPiece = b.squares[m.getFrom()];
	const int toPiece = b.squares[m.getTo()];

	assert(fromPiece != EMPTY);

	const int fromType = pieceType(fromPiece);
	const int toType = pieceType(toPiece);	

	// Save board state in undo object
	Undo u(b.key, b.epSquare, b.castlingRights, b.fiftyMove, b.psqt, toPiece);

	b.fiftyMove = (fromType == PAWN || toPiece != EMPTY) ? 0 : b.fiftyMove + 1;

	b.squares[m.getFrom()] = EMPTY;
	b.squares[m.getTo()] = fromPiece;

	b.pieces[fromType] ^= (1ull << m.getFrom()) ^ (1ull << m.getTo());
	b.colors[b.turn] ^= (1ull << m.getFrom()) ^ (1ull << m.getTo());
	if (toPiece != EMPTY) {
		b.pieces[toType] ^= (1ull << m.getTo());
		b.colors[!b.turn] ^= (1ull << m.getTo());
	}
	b.colors[NO_COLOR] = ~(b.colors[b.turn] | b.colors[!b.turn]);

	b.key ^= pieceKeys[fromPiece][m.getFrom()] ^ pieceKeys[fromPiece][m.getTo()];
	if (toPiece != EMPTY) b.key ^= pieceKeys[toPiece][m.getTo()];
	b.key ^= turnKey;

	// Update en passant square
	if (fromType == PAWN && (m.getFrom() ^ m.getTo()) == 16) {
		if (b.pieces[PAWN] & b.colors[!b.turn] & ((b.turn) ? rank5Mask : rank4Mask)) {
			b.epSquare = (b.turn) ? m.getFrom() + S : m.getFrom() + N;
			b.key ^= epKeys[b.epSquare % 8];
		}
	}
	if (b.epSquare == u.epSquare) b.epSquare = -1;

	// Update piece-square tables
	int psqt = psqtScore(fromType, psqtSquare(m.getTo(), b.turn), MG) - psqtScore(fromType, psqtSquare(m.getFrom(), b.turn), MG);
	if (toPiece != EMPTY) psqt += psqtScore(toType, psqtSquare(m.getTo(), !b.turn), MG);
	b.psqt += (b.turn == WHITE) ? psqt : -psqt;

	updateCastleRights(b, m);

	return u;
}

Undo makeEnPassantMove(Board& b, const Move& m) {
	const int fromPiece = b.squares[m.getFrom()];
	const int epPiece = (b.turn) ? W_PAWN : B_PAWN;
	const int epSquare = b.epSquare - 8 + (b.turn << 4);

	assert(pieceType(fromPiece) == PAWN);
	assert(b.squares[m.getTo()] == EMPTY);
	assert(b.squares[epSquare] == epPiece);

	Undo u(b.key, b.epSquare, b.castlingRights, b.fiftyMove, b.psqt, epPiece);

	b.fiftyMove = 0;

	b.squares[m.getFrom()] = EMPTY;
	b.squares[m.getTo()] = fromPiece;
	b.squares[epSquare] = EMPTY;

	b.pieces[PAWN] ^= (1ull << m.getFrom()) ^ (1ull << m.getTo());
	b.colors[b.turn] ^= (1ull << m.getFrom()) ^ (1ull << m.getTo());

	b.pieces[PAWN] ^= (1ull << epSquare);
	b.colors[!b.turn] ^= (1ull << epSquare);

	b.colors[NO_COLOR] = ~(b.colors[b.turn] | b.colors[!b.turn]);

	b.key ^= pieceKeys[fromPiece][m.getFrom()] ^ pieceKeys[fromPiece][m.getTo()] ^ pieceKeys[epPiece][epSquare];
	b.key ^= turnKey;

	int psqt = psqtScore(PAWN, psqtSquare(m.getTo(), b.turn), MG) - psqtScore(PAWN, psqtSquare(m.getFrom(), b.turn), MG) + psqtScore(PAWN, psqtSquare(epSquare, !b.turn), MG);
	b.psqt += (b.turn == WHITE) ? psqt : -psqt;

	return u;
}

Undo makePromotionMove(Board& b, const Move& m) {
	const int fromPiece = b.squares[m.getFrom()];
	const int toPiece = b.squares[m.getTo()];

	const int fromType = pieceType(fromPiece);
	const int toType = pieceType(toPiece);

	int promotionPiece = EMPTY;
	switch (m.getFlag()) {
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
	Undo u(b.key, b.epSquare, b.castlingRights, b.fiftyMove, b.psqt, toPiece);

	b.fiftyMove = 0;

	b.squares[m.getFrom()] = EMPTY;
	b.squares[m.getTo()] = promotionPiece;

	b.pieces[fromType] ^= (1ull << m.getFrom());
	b.pieces[pieceType(promotionPiece)] ^= (1ull << m.getTo());
	b.colors[b.turn] ^= (1ull << m.getFrom()) ^ (1ull << m.getTo());
	if (toPiece != EMPTY) {
		b.pieces[toType] ^= (1ull << m.getTo());
		b.colors[!b.turn] ^= (1ull << m.getTo());
	}
	b.colors[NO_COLOR] = ~(b.colors[b.turn] | b.colors[!b.turn]);

	b.key ^= pieceKeys[fromPiece][m.getFrom()] ^ pieceKeys[promotionPiece][m.getTo()];
	if (toPiece != EMPTY) b.key ^= pieceKeys[toPiece][m.getTo()];
	b.key ^= turnKey;

	int psqt = psqtScore(pieceType(promotionPiece), psqtSquare(m.getTo(), b.turn), MG) - psqtScore(PAWN, psqtSquare(m.getFrom(), b.turn), MG);
	if (toPiece != EMPTY) psqt += psqtScore(toType, psqtSquare(m.getTo(), !b.turn), MG);
	b.psqt += (b.turn == WHITE) ? psqt : -psqt;

	return u;
}

Undo makeCastleMove(Board& b, const Move& m) {
	const int fromPiece = b.squares[m.getFrom()];
	const int fromType = pieceType(fromPiece);

	const int fromRook = m.getFrom() + ((m.getTo() == G1 || m.getTo() == G8) ? 3 : -4);
	const int toRook = m.getFrom() + ((m.getTo() == G1 || m.getTo() == G8) ? 1 : -1);

	const int rookPiece = makePiece(ROOK, b.turn);

	assert(fromType == KING);

	// Save board state in undo object
	Undo u(b.key, b.epSquare, b.castlingRights, b.fiftyMove, b.psqt, EMPTY);

	b.fiftyMove += 1;

	b.squares[m.getFrom()] = EMPTY;
	b.squares[m.getTo()] = fromPiece;
	b.squares[fromRook] = EMPTY;
	b.squares[toRook] = rookPiece;

	b.pieces[KING] ^= (1ull << m.getFrom()) ^ (1ull << m.getTo());
	b.pieces[ROOK] ^= (1ull << fromRook) ^ (1ull << toRook);
	b.colors[b.turn] ^= (1ull << m.getFrom()) ^ (1ull << m.getTo()) ^ (1ull << fromRook) ^ (1ull << toRook);

	b.colors[NO_COLOR] = ~(b.colors[b.turn] | b.colors[!b.turn]);

	b.key ^= pieceKeys[fromPiece][m.getFrom()] ^ pieceKeys[fromPiece][m.getTo()] ^ pieceKeys[rookPiece][fromRook] ^ pieceKeys[rookPiece][toRook];
	b.key ^= turnKey;

	updateCastleRights(b, m);

	int psqt = psqtScore(ROOK, psqtSquare(toRook, b.turn), MG) - psqtScore(ROOK, psqtSquare(fromRook, b.turn), MG);
	b.psqt += (b.turn == WHITE) ? psqt : -psqt;

	return u;
}

void undoMove(Board& b, const Move& m, const Undo& u) {
	b.turn = !b.turn;
	b.key = u.key;
	b.epSquare = u.epSquare;
	b.castlingRights = u.castlingRights;
	b.psqt = u.psqt;
	b.fiftyMove = u.fiftyMove;
	b.history[b.moveNum--] = 0ull;

	assert(validSquare(m.getFrom()) && validSquare(m.getTo()));

	if (m.getFlag() == EP_MOVE) {
		const int epSquare = b.epSquare - 8 + (b.turn << 4);

		b.squares[m.getFrom()] = b.squares[m.getTo()];
		b.squares[m.getTo()] = EMPTY;
		b.squares[epSquare] = u.capturedPiece;

		b.pieces[PAWN] ^= (1ull << m.getFrom()) ^ (1ull << m.getTo());
		b.colors[b.turn] ^= (1ull << m.getFrom()) ^ (1ull << m.getTo());

		b.pieces[PAWN] ^= (1ull << epSquare);
		b.colors[!b.turn] ^= (1ull << epSquare);
	}
	else if (m.getFlag() >= PROMOTION_KNIGHT) {
		const int capturedType = pieceType(u.capturedPiece);

		b.squares[m.getFrom()] = makePiece(PAWN, b.turn);
		b.squares[m.getTo()] = u.capturedPiece;

		int promotionPiece = makePiece(m.getFlag() - 2, b.turn);

		b.pieces[PAWN] ^= (1ull << m.getFrom());
		b.pieces[pieceType(promotionPiece)] ^= (1ull << m.getTo());
		b.colors[b.turn] ^= (1ull << m.getFrom()) ^ (1ull << m.getTo());
		if (u.capturedPiece != EMPTY) {
			b.pieces[capturedType] ^= (1ull << m.getTo());
			b.colors[!b.turn] ^= (1ull << m.getTo());
		}
	}
	else if (m.getFlag() == CASTLE_MOVE) {
		const int fromRook = m.getFrom() + ((m.getTo() == G1 || m.getTo() == G8) ? 3 : -4);
		const int toRook = m.getFrom() + ((m.getTo() == G1 || m.getTo() == G8) ? 1 : -1);

		b.squares[m.getFrom()] = b.squares[m.getTo()];
		b.squares[m.getTo()] = EMPTY;
		b.squares[fromRook] = b.squares[toRook];
		b.squares[toRook] = EMPTY;

		b.pieces[KING] ^= (1ull << m.getFrom()) ^ (1ull << m.getTo());
		b.pieces[ROOK] ^= (1ull << fromRook) ^ (1ull << toRook);
		b.colors[b.turn] ^= (1ull << m.getFrom()) ^ (1ull << m.getTo()) ^ (1ull << fromRook) ^ (1ull << toRook);
	}
	else {  // NORMAL_MOVE
		b.squares[m.getFrom()] = b.squares[m.getTo()];
		b.squares[m.getTo()] = u.capturedPiece;

		const int fromType = pieceType(b.squares[m.getFrom()]);
		const int toType = pieceType(b.squares[m.getTo()]);

		b.pieces[fromType] ^= (1ull << m.getFrom()) ^ (1ull << m.getTo());
		b.colors[b.turn] ^= (1ull << m.getFrom()) ^ (1ull << m.getTo());
		if (u.capturedPiece != EMPTY) {
			b.pieces[toType] ^= (1ull << m.getTo());
			b.colors[!b.turn] ^= (1ull << m.getTo());
		}
	}

	b.colors[NO_COLOR] = ~(b.colors[b.turn] | b.colors[!b.turn]);
}

Undo makeNullMove(Board& b) {
	Undo u(b.key, b.epSquare, b.castlingRights, b.fiftyMove, b.psqt, EMPTY);
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

void updateCastleRights(Board& b, const Move& m) {
	if (b.castlingRights & WK_CASTLING) {
		if ((b.squares[m.getFrom()] == W_KING) ||
			(m.getTo() == E1 || m.getTo() == H1) ||
			(m.getFrom() == H1 && b.squares[H1] == W_ROOK))
		{
			b.castlingRights ^= WK_CASTLING;
		}
	}
	if (b.castlingRights & WQ_CASTLING) {
		if ((b.squares[m.getFrom()] == W_KING) ||
			(m.getTo() == E1 || m.getTo() == A1) ||
			(m.getFrom() == A1 && b.squares[A1] == W_ROOK))
		{
			b.castlingRights ^= WQ_CASTLING;
		}
	}
	if (b.castlingRights & BK_CASTLING) {
		if ((b.squares[m.getFrom()] == B_KING) ||
			(m.getTo() == E8 || m.getTo() == H8) ||
			(m.getFrom() == H8 && b.squares[H8] == B_ROOK))
		{
			b.castlingRights ^= BK_CASTLING;
		}
	}
	if (b.castlingRights & BQ_CASTLING) {
		if ((b.squares[m.getFrom()] == B_KING) ||
			(m.getTo() == E8 || m.getTo() == A8) ||
			(m.getFrom() == A8 && b.squares[A8] == B_ROOK))
		{
			b.castlingRights ^= BQ_CASTLING;
		}
	}
}

bool moveIsPsuedoLegal(const Board& b, const Move& m) {
	// Null move
	if (m == NO_MOVE) return false;

	const int from = m.getFrom();
	const int to = m.getTo();
	const int flag = m.getFlag();

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