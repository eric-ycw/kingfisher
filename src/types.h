#ifndef TYPES_H
#define TYPES_H

#include <assert.h>
#include <stdint.h>
#include <time.h>

#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

enum Game { MAX_MOVES = 1024 };

enum Color { WHITE, BLACK, NO_COLOR };

enum PieceType { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING };

enum Piece {
	W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
	B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
	EMPTY = 12
};

enum Square {
	A1, B1, C1, D1, E1, F1, G1, H1,
	A2, B2, C2, D2, E2, F2, G2, H2,
	A3, B3, C3, D3, E3, F3, G3, H3,
	A4, B4, C4, D4, E4, F4, G4, H4,
	A5, B5, C5, D5, E5, F5, G5, H5,
	A6, B6, C6, D6, E6, F6, G6, H6,
	A7, B7, C7, D7, E7, F7, G7, H7,
	A8, B8, C8, D8, E8, F8, G8, H8,
	SQUARE_NUM = 64
};

enum File { FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, NO_FILE };

enum Rank { RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, NO_RANK };

enum Direction {
	N = 8,
	E = 1,
	S = -N,
	W = -E,

	NE = N + E,
	NW = N + W,
	SE = S + E,
	SW = S + W,

	NNE = N * 2 + E,
	NNW = N * 2 + W,
	SSE = S * 2 + E,
	SSW = S * 2 + W,
	NEE = N + E * 2,
	NWW = N + W * 2,
	SEE = S + E * 2,
	SWW = S + W * 2,
};

enum Castling { WK_CASTLING = 1, WQ_CASTLING = 2, BK_CASTLING = 4, BQ_CASTLING = 8 };

enum Phase { MG, EG };

enum Search {
	MAX_PLY = 64,
};

enum Score {
	MATE_SCORE = 64000,
	MATE_IN_MAX = MATE_SCORE - MAX_PLY,
	MATED_IN_MAX = -MATE_SCORE + MAX_PLY,
	NO_VALUE = MATE_SCORE + 1
};

// Forward declarations
typedef struct Board Board;
typedef struct Undo Undo;
typedef struct Move Move;
typedef struct SearchInfo SearchInfo;
typedef struct TTInfo TTInfo;

// Helper functions
static inline int toSquare(int r, int c) {
	return r * 8 + c;
}

static inline int pieceColor(int piece) {
	if (piece == EMPTY) return NO_COLOR;
	return piece > W_KING;
}

static inline int pieceType(int piece) {
	if (piece == EMPTY) return EMPTY;
	return piece - pieceColor(piece) * 6;
}

static inline int makePiece(int piece, int color) {
	return piece + color * 6;
}

static inline bool validSquare(int sqr) {
	return 0 <= sqr && sqr < SQUARE_NUM;
}

#endif