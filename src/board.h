#ifndef BOARD_H
#define BOARD_H

#include "types.h"

struct Board {
	int squares[SQUARE_NUM];
	uint64_t pieces[6];
	uint64_t colors[3];  // white, black, empty
	uint64_t key;
	uint64_t history[MAX_MOVES];
	int moveNum;
	int turn;
	int epSquare;
	int castlingRights;
	int psqt;
	int fiftyMove;
};

struct Undo {
	Undo() {
		key = 0;
		epSquare = -1;
		castlingRights = 0;
		fiftyMove = 0;
		psqt = 0;
		capturedPiece = EMPTY;
	}
	Undo(uint64_t keyParam, int epParam, int castleParam, int fiftyParam, int psqtParam, int capturedParam) {
		key = keyParam;
		epSquare = epParam;
		castlingRights = castleParam;
		fiftyMove = fiftyParam;
		psqt = psqtParam;
		capturedPiece = capturedParam;
	}
	uint64_t key;
	int epSquare;
	int castlingRights;
	int fiftyMove;
	int psqt;
	int capturedPiece;
};

extern uint64_t pieceKeys[12][SQUARE_NUM];
extern uint64_t castlingKeys[16];
extern uint64_t epKeys[8];
extern uint64_t turnKey;

static constexpr char pieceChars[13] {
	'P', 'N', 'B', 'R', 'Q', 'K', 'p', 'n', 'b', 'r', 'q', 'k', ' '
};

void initKeys();

void clearBoard(Board& b);
void setSquare(Board&b, int piece, int sqr);

bool drawnByRepetition(const Board& b);

void parseFen(Board& b, std::string fen);

std::string toNotation(int sqr);
std::string toNotation(const Move& m);
Move toMove(const Board& b, const std::string& notation);

void printBoard(const Board& b);

uint64_t perft(Board& b, int depth, int ply);

#endif