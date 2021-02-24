#include "attacks.h"
#include "bitboard.h"
#include "board.h"
#include "move.h"
#include "movegen.h"
#include "movepick.h"
#include "search.h"
#include "types.h"

uint16_t pickNextMove(const Board& b, const uint16_t& hashMove, int& stage, std::vector<ScoredMove>& moves, const int& ply, int& movesTried) {

	switch (stage) {
		
		case START_PICK: {
			stage = TT_PICK;
			[[fallthrough]];
		}

		case TT_PICK: { // We try move from hash table first
			stage = NORMAL_GEN;
			if (hashMove != 0 && moveIsPsuedoLegal(b, hashMove)) {
				movesTried++;
				return hashMove;
			}
			
			[[fallthrough]];
		}

		case NORMAL_GEN: { // We generate, score and sort the rest of the moves
			stage = NORMAL_PICK;
			bool ageHistory = false;

			auto unscoredMoves = genAllMoves(b);
			moves = scoreMoves(b, unscoredMoves, ply, hashMove);
			std::sort(moves.begin(), moves.end(), [](const auto& a, const auto& b) { return a.score > b.score; });

			[[fallthrough]];
		}
		
		case NORMAL_PICK: {
			REPICK:

			movesTried++;

			// Check if out of bounds
			if (movesTried > moves.size()) {
				assert(movesTried == moves.size() + 1);
				movesTried--;
				return 0;
			}

			// We skip the hash move, as we have already tried it
			if (moves[movesTried - 1].m == hashMove) {
				movesTried--;
				goto REPICK; // FIXME: Find a way to do this without goto
			}

			return moves[movesTried - 1].m;
		}

		default: {
			return 0;
		}

	}

}