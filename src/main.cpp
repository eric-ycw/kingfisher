#include "attacks.h"
#include "bitboard.h"
#include "board.h"
#include "evaluate.h"
#include "move.h"
#include "movegen.h"
#include "search.h"
#include "uci.h"
#include "types.h"

int main()
{
	Board b;
	SearchInfo si;
	initKeys();
	initAttacks();

	std::cout << "id name Kingfisher\n";
	std::cout << "id author Eric Yip\n";
	std::cout << "uciok\n";

	std::string input;
	while (1) {
		std::getline(std::cin, input);
		if (input == "\n" || input == "") { continue; }

		if (!input.compare(0, 7, "isready")) {
			std::cout << "readyok\n";
			continue;
		}
		else if (!input.compare(0, 8, "position")) {
			parsePosition(b,input);
		}
		else if (!input.compare(0, 10, "ucinewgame")) {
			parsePosition(b, "position startpos\n");
		}
		else if (!input.compare(0, 2, "go")) {
			parseGo(b, si, input);
		}
		else if (!input.compare(0, 3, "uci")) {
			std::cout << "id name Kingfisher\n";
			std::cout << "id author Eric Yip\n";
			std::cout << "uciok\n";
		}
		else if (!input.compare(0, 5, "perft")) {
			size_t pos = input.find("perft");
			int depth = std::stoi(input.substr(pos + 6));
			perft(b, depth, 0);
		}
		else if (!input.compare(0, 4, "quit")) {
			break;
		}
	}

	return 0;
}
