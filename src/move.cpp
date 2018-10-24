#include "bitboard.h"
#include "board.h"
#include "evaluate.h"
#include "move.h"
#include "types.h"

Undo makeMove(Board& b, const Move& m) {
	if (m == NO_MOVE) return Undo();
	assert(validSquare(m.from) && validSquare(m.to));
	Undo u;
	switch (m.flag) {
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
	const int fromPiece = b.squares[m.from];
	const int toPiece = b.squares[m.to];

	assert(fromPiece != EMPTY);

	const int fromType = pieceType(fromPiece);
	const int toType = pieceType(toPiece);	

	// Save board state in undo object
	Undo u(b.key, b.epSquare, b.castlingRights, b.fiftyMove, b.psqt, toPiece);

	b.fiftyMove = (fromType == PAWN || toPiece != EMPTY) ? 0 : b.fiftyMove + 1;

	b.squares[m.from] = EMPTY;
	b.squares[m.to] = fromPiece;

	b.pieces[fromType] ^= (1ull << m.from) ^ (1ull << m.to);
	b.colors[b.turn] ^= (1ull << m.from) ^ (1ull << m.to);
	if (toPiece != EMPTY) {
		b.pieces[toType] ^= (1ull << m.to);
		b.colors[!b.turn] ^= (1ull << m.to);
	}
	b.colors[NO_COLOR] = ~(b.colors[b.turn] | b.colors[!b.turn]);

	b.key ^= pieceKeys[fromPiece][m.from] ^ pieceKeys[fromPiece][m.to];
	if (toPiece != EMPTY) b.key ^= pieceKeys[toPiece][m.to];
	b.key ^= turnKey;

	// Update en passant square
	if (fromType == PAWN && (m.from ^ m.to) == 16) {
		if (b.pieces[PAWN] & b.colors[!b.turn] & ((b.turn) ? rank5Mask : rank4Mask)) {
			b.epSquare = (b.turn) ? m.from + S : m.from + N;
			b.key ^= epKeys[b.epSquare % 8];
		}
	}
	if (b.epSquare == u.epSquare) b.epSquare = -1;

	// Update piece-square tables
	int psqt = psqtScore(fromType, psqtSquare(m.to, b.turn), MG) - psqtScore(fromType, psqtSquare(m.from, b.turn), MG);
	if (toPiece != EMPTY) psqt += psqtScore(toType, psqtSquare(m.to, !b.turn), MG);
	b.psqt += (b.turn == WHITE) ? psqt : -psqt;

	updateCastleRights(b, m);

	return u;
}

Undo makeEnPassantMove(Board& b, const Move& m) {
	const int fromPiece = b.squares[m.from];
	const int epPiece = (b.turn) ? W_PAWN : B_PAWN;
	const int epSquare = b.epSquare - 8 + (b.turn << 4);

	assert(pieceType(fromPiece) == PAWN);
	assert(b.squares[m.to] == EMPTY);
	assert(b.squares[epSquare] == epPiece);

	Undo u(b.key, b.epSquare, b.castlingRights, b.fiftyMove, b.psqt, epPiece);

	b.fiftyMove = 0;

	b.squares[m.from] = EMPTY;
	b.squares[m.to] = fromPiece;
	b.squares[epSquare] = EMPTY;

	b.pieces[PAWN] ^= (1ull << m.from) ^ (1ull << m.to);
	b.colors[b.turn] ^= (1ull << m.from) ^ (1ull << m.to);

	b.pieces[PAWN] ^= (1ull << epSquare);
	b.colors[!b.turn] ^= (1ull << epSquare);

	b.colors[NO_COLOR] = ~(b.colors[b.turn] | b.colors[!b.turn]);

	b.key ^= pieceKeys[fromPiece][m.from] ^ pieceKeys[fromPiece][m.to] ^ pieceKeys[epPiece][epSquare];
	b.key ^= turnKey;

	int psqt = psqtScore(PAWN, psqtSquare(m.to, b.turn), MG) - psqtScore(PAWN, psqtSquare(m.from, b.turn), MG) + psqtScore(PAWN, psqtSquare(epSquare, !b.turn), MG);
	b.psqt += (b.turn == WHITE) ? psqt : -psqt;

	return u;
}

Undo makePromotionMove(Board& b, const Move& m) {
	const int fromPiece = b.squares[m.from];
	const int toPiece = b.squares[m.to];

	const int fromType = pieceType(fromPiece);
	const int toType = pieceType(toPiece);

	int promotionPiece = EMPTY;
	switch (m.flag) {
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

	b.squares[m.from] = EMPTY;
	b.squares[m.to] = promotionPiece;

	b.pieces[fromType] ^= (1ull << m.from);
	b.pieces[pieceType(promotionPiece)] ^= (1ull << m.to);
	b.colors[b.turn] ^= (1ull << m.from) ^ (1ull << m.to);
	if (toPiece != EMPTY) {
		b.pieces[toType] ^= (1ull << m.to);
		b.colors[!b.turn] ^= (1ull << m.to);
	}
	b.colors[NO_COLOR] = ~(b.colors[b.turn] | b.colors[!b.turn]);

	b.key ^= pieceKeys[fromPiece][m.from] ^ pieceKeys[promotionPiece][m.to];
	if (toPiece != EMPTY) b.key ^= pieceKeys[toPiece][m.to];
	b.key ^= turnKey;

	int psqt = psqtScore(pieceType(promotionPiece), psqtSquare(m.to, b.turn), MG) - psqtScore(PAWN, psqtSquare(m.from, b.turn), MG);
	if (toPiece != EMPTY) psqt += psqtScore(toType, psqtSquare(m.to, !b.turn), MG);
	b.psqt += (b.turn == WHITE) ? psqt : -psqt;

	return u;
}

Undo makeCastleMove(Board& b, const Move& m) {
	const int fromPiece = b.squares[m.from];
	const int fromType = pieceType(fromPiece);

	const int fromRook = m.from + ((m.to == G1 || m.to == G8) ? 3 : -4);
	const int toRook = m.from + ((m.to == G1 || m.to == G8) ? 1 : -1);

	const int rookPiece = makePiece(ROOK, b.turn);

	assert(fromType == KING);

	// Save board state in undo object
	Undo u(b.key, b.epSquare, b.castlingRights, b.fiftyMove, b.psqt, EMPTY);

	b.fiftyMove += 1;

	b.squares[m.from] = EMPTY;
	b.squares[m.to] = fromPiece;
	b.squares[fromRook] = EMPTY;
	b.squares[toRook] = rookPiece;

	b.pieces[KING] ^= (1ull << m.from) ^ (1ull << m.to);
	b.pieces[ROOK] ^= (1ull << fromRook) ^ (1ull << toRook);
	b.colors[b.turn] ^= (1ull << m.from) ^ (1ull << m.to) ^ (1ull << fromRook) ^ (1ull << toRook);

	b.colors[NO_COLOR] = ~(b.colors[b.turn] | b.colors[!b.turn]);

	b.key ^= pieceKeys[fromPiece][m.from] ^ pieceKeys[fromPiece][m.to] ^ pieceKeys[rookPiece][fromRook] ^ pieceKeys[rookPiece][toRook];
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

	assert(validSquare(m.from) && validSquare(m.to));

	if (m.flag == EP_MOVE) {
		const int epSquare = b.epSquare - 8 + (b.turn << 4);

		b.squares[m.from] = b.squares[m.to];
		b.squares[m.to] = EMPTY;
		b.squares[epSquare] = u.capturedPiece;

		b.pieces[PAWN] ^= (1ull << m.from) ^ (1ull << m.to);
		b.colors[b.turn] ^= (1ull << m.from) ^ (1ull << m.to);

		b.pieces[PAWN] ^= (1ull << epSquare);
		b.colors[!b.turn] ^= (1ull << epSquare);
	}
	else if (m.flag >= PROMOTION_KNIGHT) {
		const int capturedType = pieceType(u.capturedPiece);

		b.squares[m.from] = makePiece(PAWN, b.turn);
		b.squares[m.to] = u.capturedPiece;

		int promotionPiece = makePiece(m.flag - 2, b.turn);

		b.pieces[PAWN] ^= (1ull << m.from);
		b.pieces[pieceType(promotionPiece)] ^= (1ull << m.to);
		b.colors[b.turn] ^= (1ull << m.from) ^ (1ull << m.to);
		if (u.capturedPiece != EMPTY) {
			b.pieces[capturedType] ^= (1ull << m.to);
			b.colors[!b.turn] ^= (1ull << m.to);
		}
	}
	else if (m.flag == CASTLE_MOVE) {
		const int fromRook = m.from + ((m.to == G1 || m.to == G8) ? 3 : -4);
		const int toRook = m.from + ((m.to == G1 || m.to == G8) ? 1 : -1);

		b.squares[m.from] = b.squares[m.to];
		b.squares[m.to] = EMPTY;
		b.squares[fromRook] = b.squares[toRook];
		b.squares[toRook] = EMPTY;

		b.pieces[KING] ^= (1ull << m.from) ^ (1ull << m.to);
		b.pieces[ROOK] ^= (1ull << fromRook) ^ (1ull << toRook);
		b.colors[b.turn] ^= (1ull << m.from) ^ (1ull << m.to) ^ (1ull << fromRook) ^ (1ull << toRook);
	}
	else {  // NORMAL_MOVE
		b.squares[m.from] = b.squares[m.to];
		b.squares[m.to] = u.capturedPiece;

		const int fromType = pieceType(b.squares[m.from]);
		const int toType = pieceType(b.squares[m.to]);

		b.pieces[fromType] ^= (1ull << m.from) ^ (1ull << m.to);
		b.colors[b.turn] ^= (1ull << m.from) ^ (1ull << m.to);
		if (u.capturedPiece != EMPTY) {
			b.pieces[toType] ^= (1ull << m.to);
			b.colors[!b.turn] ^= (1ull << m.to);
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
		if ((b.squares[m.from] == W_KING) ||
			(m.to == E1 || m.to == H1) ||
			(m.from == H1 && b.squares[H1] == W_ROOK))
		{
			b.castlingRights ^= WK_CASTLING;
		}
	}
	if (b.castlingRights & WQ_CASTLING) {
		if ((b.squares[m.from] == W_KING) ||
			(m.to == E1 || m.to == A1) ||
			(m.from == A1 && b.squares[A1] == W_ROOK))
		{
			b.castlingRights ^= WQ_CASTLING;
		}
	}
	if (b.castlingRights & BK_CASTLING) {
		if ((b.squares[m.from] == B_KING) ||
			(m.to == E8 || m.to == H8) ||
			(m.from == H8 && b.squares[H8] == B_ROOK))
		{
			b.castlingRights ^= BK_CASTLING;
		}
	}
	if (b.castlingRights & BQ_CASTLING) {
		if ((b.squares[m.from] == B_KING) ||
			(m.to == E8 || m.to == A8) ||
			(m.from == A8 && b.squares[A8] == B_ROOK))
		{
			b.castlingRights ^= BQ_CASTLING;
		}
	}
}