#include "attacks.h"
#include "bitboard.h"
#include "board.h"
#include "evaluate.h"
#include "move.h"
#include "movegen.h"
#include "movepick.h"
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

void timeCheck(SearchInfo& si, const bool ignoreDepth, const bool ignoreNodeCount) {
	bool depthFlag = (ignoreDepth || (!ignoreDepth && si.depth > 1));
	bool nodeFlag = (ignoreNodeCount|| (!ignoreNodeCount && (((si.nodes + si.qnodes) & 1023) == 0)));
	if (depthFlag && nodeFlag) {
		si.abort = ((double)(clock() - si.start) / (CLOCKS_PER_SEC / 1000) >= si.limit);
	}
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

int scoreMove(const Board& b, const Move& m, int ply, int phase, const Move& hashMove, bool& ageHistory) {
	/*

	1. Hash move
	2. Good captures/promotions
	3. Equal captures (0)
	4. Killer moves
	5. History moves (-5 to -10000)
	6. Bad captures

	*/

	const int from = m.getFrom();
	const int to = m.getTo();
	const int flag = m.getFlag();

	// Hash move
	if (m == hashMove) return hashMoveBonus;

	// Noisy moves
	if (b.squares[to] != EMPTY || flag == EP_MOVE || flag >= PROMOTION_KNIGHT) {
		return (staticExchangeEvaluation(b, m)) ? scoreNoisyMove(b, m) : INT_MIN + 1;
	}

	// Killer moves
	if (m == killers[0][ply]) return killerBonus[0];
	if (m == killers[1][ply]) return killerBonus[1];
	if (ply - 2 >= 0) {
		if (m == killers[0][ply - 2]) return killerBonus[2];
		if (m == killers[1][ply - 2]) return killerBonus[3];
	}

	// History heuristic
	int historyScore = historyMoves[b.turn][pieceType(b.squares[from])][to];
	if (historyScore >= historyMax) { 
		historyScore = historyMax;
		ageHistory = true;
	}
	return (-historyMax - 5) + historyScore;
}

std::vector<Move> scoreMoves(const Board& b, const std::vector<Move>& moves, int ply, const Move& hashMove, bool& ageHistory) {
	int phase = getPhase(b);
	int size = moves.size();
	std::vector<Move> scoredMoves(size);

	for (int i = 0; i < size; ++i) {
		scoredMoves[i] = moves[i];
		scoredMoves[i].score = scoreMove(b, moves[i], ply, phase, hashMove, ageHistory);
	}
	return scoredMoves;
}

int scoreNoisyMove(const Board& b, const Move& m) {
	int fromType = pieceType(b.squares[m.getFrom()]);
	if (fromType == BISHOP) fromType = KNIGHT; // We treat knight and bishop as equals
	const int toType = (m.getFlag() == EP_MOVE) ? PAWN : pieceType(b.squares[m.getTo()]);

	// MVV-LVA
	int score = SEEValues[toType] - fromType;
	// Promotion bonus
	if (m.getFlag() >= PROMOTION_KNIGHT) {
		score += SEEValues[m.getFlag() - 2];
	}

	return score * 100;
}

std::vector<Move> scoreNoisyMoves(const Board& b, const std::vector<Move>& moves) {
	int size = moves.size();
	std::vector<Move> scoredMoves(size);
	for (int i = 0; i < size; ++i) {
		scoredMoves[i] = moves[i];
		scoredMoves[i].score = scoreNoisyMove(b, moves[i]);
	}
	return scoredMoves;
}

int search(Board& b, int depth, int ply, int alpha, int beta, SearchInfo& si, Move (&ppv)[MAX_PLY], bool allowNull = true) {
	bool isRoot = (!ply);
	bool isPV = (alpha != beta - 1);
	bool nearMate = (alpha <= MATED_IN_MAX || alpha >= MATE_IN_MAX || beta <= MATED_IN_MAX || beta >= MATE_IN_MAX);
	bool isInCheck = inCheck(b, b.turn);
	if (ply > si.seldepth) si.seldepth = ply;

	if (depth <= 0 && !isInCheck) {
		// Depth is always non-negative
		depth = 0;
		if (isInCheck) {
			// Check extension
			depth++;
		}
		else {
			// Drop into qsearch
			return qsearch(b, ply, alpha, beta, si, si.pv);
		}
	}

	// Initialize new PV
	Move pv[MAX_PLY];
	for (auto& p : pv) p = NO_MOVE;

	si.nodes++;

	timeCheck(si, false, false);
	if (si.abort) return alpha;

	int ttEval = NO_VALUE;

	if (!isRoot) {
		// Check for 3-fold repetition
		if (drawnByRepetition(b)) return 0;

		// Mate distance pruning
		int mAlpha = (alpha > -MATE_SCORE + ply) ? alpha : -MATE_SCORE + ply;
		int mBeta = (beta < MATE_SCORE - ply - 1) ? beta : MATE_SCORE - ply - 1;
		if (mAlpha >= mBeta) return mAlpha;

		// Probe transposition table
		int TTScore = probeTT(b.key, depth, alpha, beta, ply, si, ttEval);
		if (TTScore != NO_VALUE && !isPV) {
			return TTScore;
		}
	}

	int eval = (ttEval == NO_VALUE) ? evaluate(b, b.turn) : ttEval;

	// Reverse futility pruning
	if (depth <= futilityMaxDepth && !isInCheck && !isRoot && eval - futilityMargin * depth > beta) {
		return eval - futilityMargin * depth;
	}

	int score;
	int TTFlag = TT_ALPHA;
	Move bestMove = NO_MOVE;

	// Null move pruning
	if (allowNull && depth >= nullMoveMinDepth && !isInCheck) {
		auto u = makeNullMove(b);
		int nullMoveR = nullMoveBaseR + depth / 6;
		nullMoveR = std::min(nullMoveR, 4);
		score = -search(b, depth - 1 - nullMoveR, ply + 1, -beta, -beta + 1, si, pv, false);
		undoNullMove(b, u);
		if (score >= beta) return score;
	}

	// Futility pruning flag
	bool fPrune = (depth <= futilityMaxDepth && !isInCheck && !isRoot && eval + futilityMargin * depth <= alpha);

	// Initialize move picking
	int stage = START_PICK;
	int movesSearched = 0;
	int movesTried = 0;
	std::vector<Move> moves;
	Move hashMove = probeHashMove(b.key);


	while (stage != NO_MOVES_LEFT) {
		Move m = pickNextMove(b, hashMove, stage, moves, ply, movesTried);
		if (m == NO_MOVE) {
			stage = NO_MOVES_LEFT;
			break;
		}

		// Move info
		const int from = m.getFrom();
		const int to = m.getTo();
		const int flag = m.getFlag();

		// Move flags
		bool isCapture = (b.squares[to] != EMPTY || flag == EP_MOVE);
		bool isPromotion = (flag >= PROMOTION_KNIGHT);
		bool isNoisy = (isCapture || isPromotion);
		bool isKiller = (m == killers[0][ply] || m == killers[1][ply]);
		bool isHash = (m == hashMove);

		bool isDangerousPawn = (b.turn == WHITE) ? 
							   (b.squares[from] == W_PAWN && checkBit(rank6Mask | rank7Mask | rank8Mask, to)) :
							   (b.squares[from] == B_PAWN && checkBit(rank1Mask | rank2Mask | rank3Mask, to));
		

		bool canPrune = (!isNoisy && !isKiller && !isInCheck && !isDangerousPawn && !isHash);


		// Futility pruning
		if (fPrune && canPrune && movesSearched > 0) {
			continue;
		}

		// Late move pruning
		if (depth <= lateMovePruningMaxDepth && movesSearched >= lateMovePruningMove * depth && canPrune) {
			continue;
		}

		auto u = makeMove(b, m);
		if (inCheck(b, !b.turn)) {
			undoMove(b, m, u);
			continue;
		}

		movesSearched++;

		bool isCheck = inCheck(b, b.turn);
		if (isHash) si.hashCount++;

		score = alpha + 1;

		// Late move reduction
		if (depth >= lateMoveMinDepth && !isNoisy && !isInCheck && !isCheck && !isDangerousPawn && !isHash) {
			int lmrIndex = (depth > 12);

			int lateMoveR = lateMoveRTable[lmrIndex][std::min(movesSearched, 63)];

			// Decrease reduction if killer move
			if (isKiller) lateMoveR -= 1;

			lateMoveR = std::max(0, std::min(lateMoveR, depth - 1));  // Do not drop directly into qsearch
			score = -search(b, depth - lateMoveR - 1, ply + 1, -alpha - 1, -alpha, si, pv);
		}

		if (score > alpha) score = -search(b, depth - 1, ply + 1, -beta, -alpha, si, pv);

		undoMove(b, m, u);

		if (score >= beta) {
			storeTT(b.key, depth, beta, TT_BETA, eval, ply, m);

			// Update killers
			if (!isNoisy && m != killers[0][ply]) {
				killers[1][ply] = killers[0][ply];
				killers[0][ply] = m;
			}

			// Update history score
			if (!isNoisy && depth <= historyMaxDepth) historyMoves[b.turn][pieceType(b.squares[m.getFrom()])][m.getTo()] += depth * depth;

			// ### DEBUG ###
			// Update near-leaf move ordering info
			if (movesSearched <= FAIL_HIGH_MOVES && (depth < NEAR_LEAF_BOUNDARY)) si.failHigh[0][movesSearched - 1]++;

			// Update near-root move ordering info
			if (movesSearched <= FAIL_HIGH_MOVES && (depth >= NEAR_LEAF_BOUNDARY)) si.failHigh[1][movesSearched - 1]++;

			// Update hash cut info
			if (m == hashMove) si.hashCut++;

			return score;
		}

		if (score > alpha) {
			alpha = score;
			bestMove = m;
			TTFlag = TT_EXACT;

			// Copy PV from child nodes
			ppv[ply] = m;
			for (int i = ply + 1; i < MAX_PLY; ++i) {
				if (pv[i] == NO_MOVE) break;
				ppv[i] = pv[i];
			}
		}
	}

	if (!movesSearched) {
		return (isInCheck) ? -MATE_SCORE + ply : 0;
	}

	storeTT(b.key, depth, alpha, TTFlag, eval, ply, bestMove);

	if (isRoot) {
		si.score = alpha;
		si.bestMove = bestMove;
	}

	return alpha;
}

int qsearch(Board& b, int ply, int alpha, int beta, SearchInfo& si, Move (&ppv)[MAX_PLY]) {
	if (ply > si.seldepth) si.seldepth = ply;

	si.qnodes++;

	timeCheck(si, true, false);
	if (si.abort) return alpha;

	// Check for draw
	if (drawnByRepetition(b)) return 0;

	// Probe eval hash;
	int qHashEval = probeQHash(b.key);
	bool hit = (qHashEval != NO_VALUE);

	int eval = (hit) ? qHashEval : evaluate(b, b.turn);

	if (!hit) storeQHash(b.key, eval);
	if (hit) si.qHashHit++;

	if (eval >= beta) return beta;
	if (eval > alpha) alpha = eval;

	// Delta pruning
	if (eval + greatestTacticalGain(b) + deltaMargin < alpha) return eval;

	auto noisyMoves = genNoisyMoves(b);
	if (noisyMoves.empty()) return eval;

	std::vector<Move> scoredNoisyMoves = scoreNoisyMoves(b, noisyMoves);
	std::sort(scoredNoisyMoves.begin(), scoredNoisyMoves.end(), [](const auto& a, const auto& b) { return a.score > b.score; });

	Move pv[MAX_PLY];
	for (auto& m : pv) m = NO_MOVE;

	int movesSearched = 0;

	for (int i = 0, size = scoredNoisyMoves.size(); i < size; ++i) {
		Move m = scoredNoisyMoves[i];

		// Delta pruning
		if (m.getFlag() < PROMOTION_KNIGHT && eval + deltaMargin + pieceValues[pieceType(b.squares[m.getTo()])][MG] < alpha) continue;

		// Negative SEE pruning
		if (!staticExchangeEvaluation(b, m)) continue;

		auto u = makeMove(b, m);
		if (inCheck(b, !b.turn)) {
			undoMove(b, m, u);
			continue;
		}

		movesSearched++;

		int score = -qsearch(b, ply + 1, -beta, -alpha, si, pv);
		undoMove(b, m, u);

		if (score >= beta) {
			// Update qsearch move ordering info
			if (movesSearched <= FAIL_HIGH_MOVES) si.failHigh[2][movesSearched - 1]++;

			return beta;
		}
		if (score > alpha) {
			alpha = score;

			ppv[ply] = m;
			for (int i = ply + 1; i < MAX_PLY; ++i) {
				if (pv[i] == NO_MOVE) break;
				ppv[i] = pv[i];
			}
		}
	}
	return alpha;
}

int staticExchangeEvaluation(const Board& b, const Move& m, int threshold) {

	int from = m.getFrom();
	int to = m.getTo();

	int nextVictim = (m.getFlag() >= PROMOTION_KNIGHT) ? m.getFlag() - 2 : pieceType(b.squares[from]);

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
	if (m.getFlag() == EP_MOVE) occupied ^= (1ull << (b.epSquare - 8 + (b.turn << 4)));

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
	int piece = pieceType(b.squares[m.getTo()]);
	int val = (piece != EMPTY) ? SEEValues[piece] : 0;
	if (m.getFlag() >= PROMOTION_KNIGHT) val += SEEValues[pieceType(m.getFlag() - 2)] - SEEValues[PAWN];
	if (m.getFlag() == EP_MOVE) val += SEEValues[PAWN];
	return val;
}

int greatestTacticalGain(const Board& b) {
	int val = SEEValues[PAWN];
	const uint64_t enemy = b.colors[!b.turn];
	for (int i = QUEEN; i > PAWN; --i) {
		if (enemy & b.pieces[i]) {
			val = SEEValues[i];
			break;
		}
	}
	if (b.pieces[PAWN] & b.colors[b.turn] & ((b.turn == WHITE) ? RANK_7 : RANK_2)) {
		val += SEEValues[QUEEN] - SEEValues[PAWN];
	}
	return val;
}

void iterativeDeepening(Board& b, SearchInfo& si, int timeLimit) {
	SearchInfo searchCache;
	Move bestMove;
	initSearch(si);
	si.initTime(timeLimit);

	int alpha = -MATE_SCORE;
	int beta = MATE_SCORE;
	bool research = false;

	for (int i = 1; i <= MAX_PLY; ++i) {
		if (!research) searchCache = si;
		si.reset();
		si.depth = i;

		int score = search(b, i, 0, alpha, beta, si, si.pv);
		timeCheck(si, true, true);
		if (si.abort) break;
		
		if ((score <= alpha) || (score >= beta)) {
			alpha = -MATE_SCORE;
			beta = MATE_SCORE;
			i--;
			research = true;
			continue;
		}

		si.print();
		si.printSearchDebug();

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