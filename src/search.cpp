#include "attacks.h"
#include "bitboard.h"
#include "board.h"
#include "evaluate.h"
#include "move.h"
#include "movegen.h"
#include "search.h"
#include "tt.h"
#include "types.h"

void initSearch(SearchInfo& si) {
	// Reset killers
	for (int i = 0; i <= MAX_PLY; ++i) {
		killers[0][i] = NO_MOVE;
		killers[1][i] = NO_MOVE;
	}

	// Reset history scores
	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < 6; ++j) {
			for (int k = 0; k < SQUARE_NUM; k++) {
				historyMoves[i][j][k] = 0;
			}
		}
	}

	si.reset();
}

bool timeOver(const SearchInfo& si) {
	return ((double)(clock() - si.start) / (CLOCKS_PER_SEC / 1000) >= si.limit);
}

void reduceHistory() {
	for (int i = 0; i < 2; ++i) {
		for (int j = 0; j < 6; ++j) {
			for (int k = 0; k < SQUARE_NUM; k++) {
				historyMoves[i][j][k] /= 2;
			}
		}
	}
}

int scoreMove(const Board& b, const Move& m, int ply, float phase, const Move& hashMove = NO_MOVE) {
	// Hash move
	if (m == hashMove) return hashMoveBonus;

	// Noisy moves
	if (b.squares[m.to] != EMPTY || m.flag == EP_MOVE || m.flag >= PROMOTION_KNIGHT) {
		return (staticExchangeEvaluation(b, m)) ? scoreNoisyMove(b, m) : -1000;
	}

	// Killer moves
	if (m == killers[0][ply]) return killerBonus[0];
	if (m == killers[1][ply]) return killerBonus[1];
	if (ply - 2 >= 0) {
		if (m == killers[0][ply - 2]) return killerBonus[2];
		if (m == killers[1][ply - 2]) return killerBonus[3];
	}

	// History heuristic
	int historyScore = historyMoves[b.turn][pieceType(b.squares[m.from])][m.to];
	if (historyScore >= historyMax) { 
		historyScore /= 2;
		reduceHistory(); 
	}
	return historyScore;
}

std::vector<std::pair<Move, int>> scoreMoves(const Board& b, const std::vector<Move>& moves, int ply) {
	float phase = getPhase(b);
	auto it = tt.find(b.key);
	Move hashMove = (it != tt.end()) ? tt[b.key].move : NO_MOVE;
	int size = moves.size();
	std::vector<std::pair<Move, int>> scoredMoves(size);

	for (int i = 0; i < size; ++i) {
		scoredMoves[i].first = moves[i];
		scoredMoves[i].second = scoreMove(b, moves[i], ply, phase, hashMove);
	}
	return scoredMoves;
}

int scoreNoisyMove(const Board& b, const Move& m) {
	int fromType = pieceType(b.squares[m.from]);
	if (fromType == BISHOP) fromType = KNIGHT; // We treat knight and bishop as equals
	const int toType = (m.flag == EP_MOVE) ? PAWN : pieceType(b.squares[m.to]);

	// MVV-LVA
	int score = SEEValues[toType] - fromType;
	// Promotion bonus
	if (m.flag >= PROMOTION_KNIGHT) {
		score += SEEValues[m.flag - 2];
	}

	return score * 100;
}

std::vector<std::pair<Move, int>> scoreNoisyMoves(const Board& b, const std::vector<Move>& moves) {
	int size = moves.size();
	std::vector<std::pair<Move, int>> scoredMoves(size);
	for (int i = 0; i < size; ++i) {
		scoredMoves[i].first = moves[i];
		scoredMoves[i].second = scoreNoisyMove(b, moves[i]);
	}
	return scoredMoves;
}

int search(Board& b, int depth, int ply, int alpha, int beta, SearchInfo& si, bool allowNull = true) {
	if (timeOver(si)) return alpha;

	bool isRoot = (!ply);
	if (ply > si.seldepth) si.seldepth = ply;

	if (!isRoot) {
		// Check for draw
		if (drawnByRepetition(b)) return 0;

		// Mate distance pruning
		int mAlpha = (alpha > -MATE_SCORE + ply) ? alpha : -MATE_SCORE + ply;
		int mBeta = (beta < MATE_SCORE - ply - 1) ? beta : MATE_SCORE - ply - 1;
		if (mAlpha >= mBeta) return mAlpha;

		// Probe transposition table
		int TTScore = probeTT(b.key, depth, alpha, beta, si);
		if (TTScore != NO_VALUE) return TTScore;
	}

	bool isInCheck = inCheck(b, b.turn);

	if (depth <= 0) {
		depth = 0;
		if (isInCheck) {
			// Check extension
			depth++;
		}
		else {
			// Drop into qsearch
			int qscore = qsearch(b, ply, alpha, beta, si);
			return qscore;
		}
	}

	int score;
	int TTFlag = TT_ALPHA;
	float phase = getPhase(b);
	Move bestMove = NO_MOVE;

	int eval = evaluate(b, b.turn);

	// Reverse futility pruning
	if (depth <= futilityMaxDepth && !isInCheck && !isRoot && eval - futilityMargin * depth > beta) {
		return eval - futilityMargin * depth;
	}

	// Null move pruning
	if (allowNull && depth >= nullMoveMinDepth && phase > nullMovePhaseLimit && !isInCheck) {
		auto u = makeNullMove(b);
		int nullMoveR = nullMoveBaseR + depth / 6;
		nullMoveR = std::min(nullMoveR, 4);
		score = -search(b, depth - 1 - nullMoveR, ply + 1, -beta, -beta + 1, si, false);
		undoNullMove(b, u);
		if (score >= beta) return score;
	}

	// Futility pruning
	bool fPrune = false;
	if (depth <= futilityMaxDepth && !isInCheck && !isRoot) {
		fPrune = (eval + futilityMargin * depth <= alpha);
	}

	// Generate all moves
	auto moves = genAllMoves(b);
	int movesSearched = 0;

	// Score and sort moves
	std::vector<std::pair<Move, int>> scoredMoves = scoreMoves(b, moves, ply);
	std::sort(scoredMoves.begin(), scoredMoves.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

	for (int i = 0, size = scoredMoves.size(); i < size; ++i) {
		if (timeOver(si)) return alpha;

		Move m = scoredMoves[i].first;
		bool isCapture = (b.squares[m.to] != EMPTY || m.flag == EP_MOVE);
		bool isPromotion = (m.flag >= PROMOTION_KNIGHT);
		bool isNoisy = (isCapture || isPromotion);
		bool isKiller = (m == killers[0][ply] || m == killers[1][ply]);

		// Make move to see whether the move would check the enemy king
		// Not an efficient implementation but works for now
		auto u = makeMove(b, m);
		if (inCheck(b, !b.turn)) {
			undoMove(b, m, u);
			continue;
		}
		bool wouldCheck = inCheck(b, b.turn);
		undoMove(b, m, u);

		// Futility pruning
		// !isRoot and !isInCheck is implicitly checked by fPrune flag
		if (fPrune && !wouldCheck && !isNoisy && !isKiller && movesSearched > 0) {
			continue;
		}

		makeMove(b, m);

		score = alpha + 1;
		// Late move reduction
		if (depth >= lateMoveMinDepth && movesSearched >= lateMoveMinMove && !isInCheck && !isNoisy && !wouldCheck && !isKiller) {
			int lateMoveR = lateMoveBaseR + movesSearched / 24 + depth / 8;
			lateMoveR = std::min(lateMoveR, depth - 1);  // Do not drop directly into qsearch
			score = -search(b, depth - lateMoveR - 1, ply + 1, -alpha - 1, -alpha, si);
		}
		if (score > alpha) score = -search(b, depth - 1, ply + 1, -beta, -alpha, si);

		undoMove(b, m, u);

		si.nodes++;
		movesSearched++;

		if (score >= beta) {
			storeTT(b.key, depth, beta, TT_BETA);

			// Update killers
			if (!isNoisy && m != killers[0][ply]) {
				killers[1][ply] = killers[0][ply];
				killers[0][ply] = m;
			}

			// Update history score
			historyMoves[b.turn][pieceType(b.squares[m.from])][m.to] += depth * depth;

			// Update move ordering info
			si.failHigh++;
			if (movesSearched == 1) si.failHighFirst++;

			return score;
		}

		if (score > alpha) {
			alpha = score;
			bestMove = m;
			TTFlag = TT_EXACT;
		}
	}

	if (!movesSearched) {
		return (inCheck(b, b.turn)) ? -MATE_SCORE + ply : 0;
	}

	storeTT(b.key, depth, alpha, TTFlag);
	tt[b.key].move = bestMove;

	if (isRoot) {
		si.score = score;
		si.bestMove = bestMove;
	}

	return alpha;
}

int qsearch(Board& b, int ply, int alpha, int beta, SearchInfo& si) {
	if (timeOver(si)) return alpha;
	if (ply > si.seldepth) si.seldepth = ply;

	int eval = evaluate(b, b.turn);
	if (eval >= beta) return beta;
	if (eval > alpha) alpha = eval;

	auto noisyMoves = genNoisyMoves(b);
	if (noisyMoves.empty()) return eval;

	std::vector<std::pair<Move, int>> scoredNoisyMoves = scoreNoisyMoves(b, noisyMoves);
	std::sort(scoredNoisyMoves.begin(), scoredNoisyMoves.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

	for (int i = 0, size = scoredNoisyMoves.size(); i < size; ++i) {
		Move m = scoredNoisyMoves[i].first;

		// Delta pruning
		if (m.flag < PROMOTION_KNIGHT && eval + deltaMargin + pieceValues[pieceType(b.squares[m.to])][MG] < alpha) continue;

		// Negative SEE pruning
		if (!staticExchangeEvaluation(b, m)) continue;

		auto u = makeMove(b, m);
		if (inCheck(b, !b.turn)) {
			undoMove(b, m, u);
			continue;
		}

		int score = -qsearch(b, ply + 1, -beta, -alpha, si);
		undoMove(b, m, u);

		si.qnodes++;

		if (score >= beta) return beta;
		if (score > alpha) alpha = score;
	}
	return alpha;
}

int staticExchangeEvaluation(const Board& b, const Move& m, int threshold) {

	int from = m.from;
	int to = m.to;

	int nextVictim = (m.flag >= PROMOTION_KNIGHT) ? m.flag - 2 : pieceType(b.squares[from]);

	int seeVal = SEEMoveVal(b, m) - threshold;
	if (seeVal < 0) return 0;
	seeVal -= SEEValues[nextVictim];
	if (seeVal >= 0) return 1;

	int victim = pieceType(b.squares[to]);
	int victimVal = SEEValues[victim];

	uint64_t bishops = b.pieces[BISHOP] | b.pieces[QUEEN];
	uint64_t rooks = b.pieces[ROOK] | b.pieces[QUEEN];

	uint64_t occupied = ~b.colors[NO_COLOR];
	occupied ^= (1ull << from) ^ (1ull << to);
	if (m.flag == EP_MOVE) occupied ^= (1ull << (b.epSquare - 8 + (b.turn << 4)));

	int side = !b.turn;
	uint64_t allAttackers = squareAttackers(b, !side, to) | squareAttackers(b, side, to) & occupied;

	while (1) {
		uint64_t ourAttackers = allAttackers & b.colors[side];
		if (!ourAttackers) break;
		int nextVictim = smallestAttacker(b, ourAttackers);

		occupied ^= (1ull << (lsb(ourAttackers & b.pieces[nextVictim])));

		// Check for discovered diagonal attacks
		if (nextVictim == PAWN || nextVictim == BISHOP || nextVictim == QUEEN) {
			allAttackers |= getBishopAttacks(occupied, to) & bishops;
		}

		// Check for discovered horizontal/vertical attacks
		if (nextVictim == ROOK || nextVictim == QUEEN) {
			allAttackers |= getRookAttacks(occupied, to) & rooks;
		}

		side = !side;
		allAttackers &= occupied;

		seeVal = -seeVal - 1 - SEEValues[nextVictim];
		if (seeVal >= 0) {
			// If our attacking piece is a king and the enemy still has attackers left, we lose the exchange
			if (nextVictim == KING && (allAttackers & b.colors[side])) side = !side;
			break;
		}
	}

	// Side to move loses the exchange
	return b.turn != side;
}

int SEEMoveVal(const Board& b, const Move& m) {
	int piece = pieceType(b.squares[m.to]);
	int val = (piece != EMPTY) ? SEEValues[piece] : 0;
	if (m.flag >= PROMOTION_KNIGHT) val += SEEValues[pieceType(m.flag - 2)] - SEEValues[PAWN];
	if (m.flag == EP_MOVE) val += SEEValues[PAWN];
	return val;
}

void iterativeDeepening(Board& b, SearchInfo& si, int timeLimit) {
	SearchInfo searchCache;
	initSearch(si);
	si.initTime(timeLimit);

	int alpha = -MATE_SCORE;
	int beta = MATE_SCORE;
	bool research = false;

	for (int i = 1; i <= MAX_PLY; ++i) {
		if (!research) searchCache = si;  // We only use best move from full searches
		si.reset();
		si.depth = i;

		int score = search(b, i, 0, alpha, beta, si);
		if (timeOver(si)) break;

		if ((score <= alpha) || (score >= beta)) {
			alpha = -MATE_SCORE;
			beta = MATE_SCORE;
			i--;
			research = true;
			continue;
		}

		si.print();
		printPV(b, i);
		std::cout << "\n";

		research = false;
		if (i >= aspirationMinDepth) {
			alpha = si.score - aspirationWindow;
			beta = si.score + aspirationWindow;
		}
	}

	si = searchCache;
	std::cout << "bestmove " << toNotation(si.bestMove) << "\n";
	ageTT();
}

void printPV(Board b, int limit) {
	int ply = 0;
	std::cout << "pv ";
	auto it = tt.find(b.key);
	Move hashMove = (it != tt.end()) ? it->second.move : NO_MOVE;
	while (hashMove != NO_MOVE) {
		ply++;
		makeMove(b, hashMove);
		std::cout << toNotation(hashMove) << " ";
		it = tt.find(b.key);
		hashMove = (it != tt.end()) ? it->second.move : NO_MOVE;
		// PV currently does not account for threefold repetition and will cycle hash moves
		// We use a failsafe to prevent an infinite loop
		if (ply > limit) break;
	}
}