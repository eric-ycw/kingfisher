#include "attacks.h"
#include "bitboard.h"
#include "board.h"
#include "evaluate.h"
#include "move.h"
#include "movegen.h"
#include "tt.h"
#include "types.h"

uint64_t pieceKeys[12][SQUARE_NUM];
uint64_t castlingKeys[16];
uint64_t epKeys[8];
uint64_t turnKey;

static uint64_t rand64() {
	std::random_device rd;
	std::mt19937_64 gen(rd());
	std::uniform_int_distribution<long long int> dist(std::llround(std::pow(2, 61)), std::llround(std::pow(2, 62)));
	return dist(gen);
}

void initKeys() {
	for (int p = W_PAWN; p <= B_KING; ++p) {
		for (int sqr = 0; sqr < SQUARE_NUM; ++sqr) {
			pieceKeys[p][sqr] = rand64();
		}
	}
	for (int i = 0; i < 16; ++i) {
		castlingKeys[i] = rand64();
	}
	for (int i = 0; i < 8; ++i) {
		epKeys[i] = rand64();
	}
	turnKey = rand64();
}

void clearBoard(Board& b) {
	memset(&b, 0, sizeof(b));
	for (auto& i : b.squares) i = EMPTY;
	b.epSquare = -1;
	b.colors[NO_COLOR] = ~0ull;
}

void setSquare(Board& b, int piece, int sqr) {
	assert(validSquare(sqr));
	b.squares[sqr] = piece;
	b.key ^= pieceKeys[piece][sqr];

	setBit(b.colors[pieceColor(piece)], sqr);
	setBit(b.pieces[pieceType(piece)], sqr);
	clearBit(b.colors[NO_COLOR], sqr);
}

bool drawnByRepetition(const Board& b) {
	int count = 0;
	for (int i = b.moveNum - 2; i >= 0; i -= 2) {
		if (b.history[i] == b.key && (++count == 2)) return true;
	}
	return false;
}

void parseFen(Board& b, std::string fen) {
	if (fen.empty()) return;
	clearBoard(b);

	int sqr = A8;
	int pos = 0;

	// Pieces and squares
	for (int i = 0, size = fen.size(); i < size; ++i) {
		auto chr = fen[i];
		if (chr == ' ') {
			pos = i + 1;
			break;
		}
		if (isdigit(chr)) {
			sqr += chr - '0';
		}
		else if (chr == '/') {
			sqr -= 16;
		}
		else {
			for (int j = 0; j < 12; ++j) {
				if (pieceChars[j] == chr) {
					setSquare(b, j, sqr);
					int color = pieceColor(j);
					int type = pieceType(j);
					if (type != KING) {
						int score = psqtScore(type, psqtSquare(sqr, color), MG);
						b.psqt += (color == WHITE) ? score : -score;
					}
					sqr++;
					break;
				}
			}
		}
	}

	// Turn
	assert(fen[pos] == 'w' || fen[pos] == 'b');
	fen.erase(0, pos);
	b.turn = (fen[0] == 'w') ? WHITE : BLACK;
	if (b.turn == BLACK) b.key ^= turnKey;

	// Castling rights
	for (int i = 2; i < 8; ++i) {
		if (fen[i] == ' ') { 
			pos = i + 1;
			break; 
		}
		switch (fen[i]) {
		case 'K': b.castlingRights ^= WK_CASTLING; break;
		case 'Q': b.castlingRights ^= WQ_CASTLING; break;
		case 'k': b.castlingRights ^= BK_CASTLING; break;
		case 'q': b.castlingRights ^= BQ_CASTLING; break;
		default: break;
		}
	}
	b.key ^= castlingKeys[b.castlingRights];

	// En passant square
	fen.erase(0, pos);
	pos = 1;
	if (fen[0] != '-') {
		pos++;
		b.epSquare = (fen[1] - '1') * 8 + (fen[0] - 'a');
		b.key ^= epKeys[b.epSquare % 8];
	}
	else {
		b.epSquare = -1;
	}

	// Fifty move rule
	fen.erase(0, pos);
	b.fiftyMove = atoi(fen.c_str());

	// Initialize move history
	b.history[0] = b.key;
}

std::string toNotation(int sqr) {
	std::string s = "  ";
	s[0] = char(sqr % 8 + 97);
	s[1] = char(sqr / 8 + 49);
	return s;
}

std::string toNotation(const Move& m) {
	if (m == NO_MOVE) return "0000";
	std::string s = "      ";
	s[0] = char(m.getFrom() % 8 + 97);
	s[1] = char(m.getFrom() / 8 + 49);
	s[2] = char(m.getTo() % 8 + 97);
	s[3] = char(m.getTo() / 8 + 49);
	switch (m.getFlag()) {
	default:
		s.resize(4);
		break;
	case PROMOTION_KNIGHT:
		s[4] = 'n';
		break;
	case PROMOTION_BISHOP:
		s[4] = 'b';
		break;
	case PROMOTION_ROOK:
		s[4] = 'r';
		break;
	case PROMOTION_QUEEN:
		s[4] = 'q';
		break;
	}
	return s;
}

Move toMove(const Board& b, const std::string& notation) {
	assert(notation.size() >= 4);
	int from = int(notation[0]) - 97 + (int(notation[1]) - 49) * 8;
	int to = int(notation[2]) - 97 +  (int(notation[3]) - 49) * 8;
	int flag = NORMAL_MOVE;
	if (pieceType(b.squares[from]) == PAWN && to == b.epSquare) flag = EP_MOVE;
	if (pieceType(b.squares[from]) == KING && abs(from - to) == 2) flag = CASTLE_MOVE;
	if (pieceType(b.squares[from]) == PAWN && (1ull << to & (rank1Mask | rank8Mask))) {
		// Default to queen promotion
		if (notation.size() == 4) {
			flag = PROMOTION_QUEEN;
		}
		else {
			if (notation[4] == 'n') flag = PROMOTION_KNIGHT;
			if (notation[4] == 'b') flag = PROMOTION_BISHOP;
			if (notation[4] == 'r') flag = PROMOTION_ROOK;
			if (notation[4] == 'q') flag = PROMOTION_QUEEN;
		}
	}
	return Move(from, to, flag);
}

void printBoard(const Board& b) {
	static const std::string separator = "\n   +---+---+---+---+---+---+---+---+\n";
	for (int rank = 7; rank >= 0; --rank) {
		std::cout << separator;
		std::cout << "   ";
		for (int file = 0; file < 8; ++file) {
			int piece = b.squares[toSquare(rank, file)];
			std::cout << "| " << pieceChars[piece] << " ";
		}
		std::cout << "|";
	}
	std::cout << separator << "\n";
	if (b.turn == WHITE) {
		std::cout << "White";
	}
	else {
		std::cout << "Black";
	}
	std::cout << " to move\n";
	std::cout << "Key: " << b.key << "\n";
	std::cout << "Castling rights: " << b.castlingRights << "\n";
	std::cout << "En passant square: " << b.epSquare << "\n";
	std::cout << "Fifty move counter: " << b.fiftyMove << "\n";
	std::cout << "Piece-square table score: " << b.psqt << "\n";
	std::cout << "Evaluation: " << evaluate(b, WHITE) << "\n";
	std::cout << "\n";
}

uint64_t perft(Board& b, int depth, int ply) {
	auto start = (!ply) ? clock() : 0;
	if (depth == 0) return 1ull;
	// Use transposition table
	int pttnodes = probePTT(b.key, depth);
	if (pttnodes != NO_NODES) {
		return pttnodes;
	}
	uint64_t count = 0ull;
	auto moves = genAllMoves(b);
	bool haveMove = false;
	for (int i = 0, size = moves.size(); i < size; ++i) {
		auto u = makeMove(b, moves[i]);
		if (!inCheck(b, !b.turn)) {
			uint64_t nodes = perft(b, depth - 1, ply + 1);
			count += nodes;
			haveMove = true;
			if (!ply) std::cout << toNotation(moves[i]) << " " << nodes << "\n";
		}
		undoMove(b, moves[i], u);
	}
	if (haveMove) { storePTT(b.key, depth, count); }
	if (!ply) std::cout << "Nodes: " << count << "\n";
	if (!ply) std::cout << "Time: " << (double)(clock() - start) / (CLOCKS_PER_SEC / 1000) << "\n";
	return (haveMove) ? count : 0ull;
}