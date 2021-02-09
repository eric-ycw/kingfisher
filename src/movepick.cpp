#include "attacks.h"
#include "bitboard.h"
#include "board.h"
#include "move.h"
#include "movegen.h"
#include "movepick.h"
#include "search.h"
#include "types.h"

Move pickNextMove(const Board& b, const Move& hashMove, int& stage, std::vector<Move>& moves, const int& ply, int& movesSearched) {

	switch (stage) {
		
		case START_PICK: {
			stage = TT_PICK;
			[[fallthrough]];
		}

		case TT_PICK: { // We try move from hash table first
			stage = NORMAL_GEN;
			if (hashMove != NO_MOVE && moveIsPsuedoLegal(b, hashMove)) {
				movesSearched++;
				return hashMove;
			}
			
			[[fallthrough]];
		}

		case NORMAL_GEN: { // We generate, score and sort the rest of the moves
			stage = NORMAL_PICK;
			moves.clear();
			auto unscoredMoves = genAllMoves(b);
			moves = scoreMoves(b, unscoredMoves, ply);
			std::sort(moves.begin(), moves.end(), [](const auto& a, const auto& b) { return a.score > b.score; });

			[[fallthrough]];
		}
		
		case NORMAL_PICK: {
			REPICK:

			movesSearched++;

			// Check if out of bounds
			if (movesSearched > moves.size() - 1) {
				assert(movesSearched == moves.size());
				return NO_MOVE;
			}

			Move m = moves[movesSearched - 1];
			if (!moveIsPsuedoLegal(b, m)) {
				printBoard(b);
				std::cout << toNotation(m) << "\n";
				assert(false);
			}

			// We skip the hash move
			if (moves[movesSearched - 1] == hashMove) goto REPICK; // FIXME: Find a way to do this without goto

			return moves[movesSearched - 1];
		}

		default: {
			return NO_MOVE;
		}

	}

}