#include "attacks.h"
#include "bitboard.h"
#include "board.h"
#include "evaluate.h"
#include "move.h"
#include "movegen.h"
#include "search.h"
#include "types.h"
#include "uci.h"

void parsePosition(Board& b, std::string input) {
	// Erase "position "
	input.erase(0, 9);

	if (!input.compare(0, 8, "startpos")) {
		parseFen(b, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
	}
	else {
		input.erase(0, 4);  // Erase "fen "
		parseFen(b, input);
	}

	size_t pos = input.find("moves");
	if (pos == std::string::npos) return;
	input.erase(0, pos + 6);  // Erase "moves "
	// Failsafe if last character is not newline
	if (input.back() != '\n') {
		input.insert(input.end(), '\n');
	}
	input.insert(input.end() - 1, ' ');  // Insert space after the last move so it can be parsed

	while ((pos = input.find(' ')) != std::string::npos) {
		makeMove(b, toMove(b, input.substr(0, pos)));
		input.erase(0, pos + 1);
	}
}
void parseGo(Board& b, SearchInfo& si, std::string input) {
	int depth = 0, moveTime = 0, time = 0;
	int moveNum = 40;
	int increment = 0, timeLeft = 0;

	size_t pos;
	if ((pos = input.find("winc")) != std::string::npos && b.turn == WHITE) {
		increment = std::stoi(input.substr(pos + 5));
	}
	if ((pos = input.find("binc")) != std::string::npos && b.turn == BLACK) {
		increment = std::stoi(input.substr(pos + 5));
	}
	if ((pos = input.find("wtime")) != std::string::npos && b.turn == WHITE) {
		time = std::stoi(input.substr(pos + 6));
	}
	if ((pos = input.find("btime")) != std::string::npos && b.turn == BLACK) {
		time = std::stoi(input.substr(pos + 6));
	}
	if ((pos = input.find("depth")) != std::string::npos) {
		depth = std::stoi(input.substr(pos + 6));
	}
	if ((pos = input.find("movestogo")) != std::string::npos) {
		moveNum = std::stoi(input.substr(pos + 10));
	}
	if ((pos = input.find("movetime")) != std::string::npos) {
		moveTime = std::stoi(input.substr(pos + 9));
	}

	if (moveTime) {
		time = moveTime;
		moveNum = 1;
	}

	if (time) {
		timeLeft = time;
		time /= moveNum;
		time -= 100;
	}

	if (!depth) {
		depth = MAX_PLY;
	}

	double total = time + increment * 0.8;
	if (timeLeft && total > timeLeft - 1000) {
		total = std::min(timeLeft * 0.75, total * 0.75);
	}
	iterativeDeepening(b, si, total);
}