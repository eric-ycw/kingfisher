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
	// We check for time every 1024 nodes to reduce calls to clock()
	bool depthFlag = (ignoreDepth || (!ignoreDepth && si.depth > 1));
	bool nodeFlag = (ignoreNodeCount|| (!ignoreNodeCount && (((si.nodes + si.qnodes) & 1023) == 0)));
	if (depthFlag && nodeFlag) {
		si.abort = ((double)(clock() - si.start) / (CLOCKS_PER_SEC / 1000) >= si.limit);
	}
}

void updateHistory(int& entry, int delta) {
	// We use an arithmetico–geometric sequence to bound the history score
	entry += historyMultiplier * delta - entry * abs(delta) / historyDivisor;
}

int scoreMove(const Board& b, const Move& m, int ply, int phase, const Move& hashMove) {
	
	// We order our moves before we search them in order to maximize the chance of causing a beta-cutoff in the first few moves searched

	// 1. Hash move (score = INT_MAX - 1)
	// 2. Good captures/promotions
	// 3. Equal captures (score = 0)
	// 4. Killer moves (score = -1 to -4)
	// 5. History moves
	// 6. Bad captures (score = INT_MIN + 1)

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
	return (-historyMax - 5) + historyScore;
}

std::vector<ScoredMove> scoreMoves(const Board& b, const std::vector<Move>& moves, int ply, const Move& hashMove) {
	int phase = getPhase(b);
	int size = moves.size();
	std::vector<ScoredMove> scoredMoves(size);

	for (int i = 0; i < size; ++i) {
		scoredMoves[i].m = moves[i];
		scoredMoves[i].score = scoreMove(b, moves[i], ply, phase, hashMove);
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

std::vector<ScoredMove> scoreNoisyMoves(const Board& b, const std::vector<Move>& moves) {
	int size = moves.size();
	std::vector<ScoredMove> scoredMoves(size);
	for (int i = 0; i < size; ++i) {
		scoredMoves[i].m = moves[i];
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

	// Step 0: Leaf node
	// We either drop into quiescence search, or extend the search by 1 ply if we are in check
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
		// Step 2: Check for 3-fold repetition
		if (drawnByRepetition(b)) return 0;

		// Step 3: Mate distance pruning
		// We have already found mate, so we can prune irrevelant branches that have no chance of giving a shorter mate
		// Does not increase playing strength, but greatly reduces tree sizes in mating positions
		int mAlpha = (alpha > -MATE_SCORE + ply) ? alpha : -MATE_SCORE + ply;
		int mBeta = (beta < MATE_SCORE - ply - 1) ? beta : MATE_SCORE - ply - 1;
		if (mAlpha >= mBeta) return mAlpha;

		// Step 4a: Probe transposition table for cutoff
		// If we have already visited this position, we might be able to get a cutoff early
		int TTScore = probeTT(b.key, depth, alpha, beta, ply, si, ttEval);
		if (TTScore != NO_VALUE && !isPV) {
			return TTScore;
		}
	}

	// Step 4b: Probe transposition table for evaluation score
	// If we have already visited this position, we can reuse the evaluation score without having to calculate it again
	int eval = (ttEval == NO_VALUE) ? evaluate(b, b.turn) : ttEval;

	// Step 5: Reverse futility pruning / Static null move pruning
	// Our static evaluation score is so good that we can still cause a beta cutoff even after deducting a safety margin
	if (depth <= futilityMaxDepth && !isInCheck && !isRoot && eval - futilityMargin * depth > beta) {
		return eval - futilityMargin * depth;
	}

	int score;
	int TTFlag = TT_ALPHA;
	Move bestMove = NO_MOVE;

	// Step 6: Null move pruning
	// If we don't make a move, and a reduced search still causes a beta cutoff, we do a cutoff immediately
	// We do not use null move pruning in pawn endgames as it would fail in zugzwang
	if (allowNull && depth >= nullMoveMinDepth && !isInCheck && getPhase(b) != 0) {
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
	std::vector<ScoredMove> moves;
	Move hashMove = probeHashMove(b.key);


	while (stage != NO_MOVES_LEFT) {
		// Step 7: We pick our next move from an ordered list of moves
		Move m = pickNextMove(b, hashMove, stage, moves, ply, movesTried);
		if (m == NO_MOVE) {
			stage = NO_MOVES_LEFT;
			break;
		}

		// Move info
		const int from = m.getFrom();
		const int to = m.getTo();
		const int flag = m.getFlag();
		const int historyScore = historyMoves[b.turn][pieceType(b.squares[from])][to];

		// Move flags
		bool isCapture = (b.squares[to] != EMPTY || flag == EP_MOVE);
		bool isPromotion = (flag >= PROMOTION_KNIGHT);
		bool isNoisy = (isCapture || isPromotion);
		bool isKiller = (m == killers[0][ply] || m == killers[1][ply]);
		bool isHash = (m == hashMove);

		bool isPassedPawn = (b.turn == WHITE) ?
							(b.squares[from] == W_PAWN && isPassed(b, from, WHITE)) :
							(b.squares[from] == B_PAWN && isPassed(b, from, BLACK));
		bool isDangerousPawn = (b.turn == WHITE) ? 
							   (b.squares[from] == W_PAWN && checkBit(rank6Mask | rank7Mask | rank8Mask, to)) :
							   (b.squares[from] == B_PAWN && checkBit(rank1Mask | rank2Mask | rank3Mask, to));
		

		bool canPrune = (!isNoisy && !isKiller && !isInCheck && !isPassedPawn && !isDangerousPawn && !isHash);


		// Step 8: Futility pruning
		// If this move has no potential of increasing alpha as defined by a safety margin, we do not search it
		if (canPrune && fPrune && movesSearched > 0) {
			continue;
		}

		// Step 9: Late move pruning / Move count pruning
		// The moves at the back of the move list are probably bad, so we skip them
		if (canPrune && depth <= lateMovePruningMaxDepth && movesSearched >= lateMovePruningMove * depth) {
			continue;
		}

		// Step 10: Make move and check for legality
		auto u = makeMove(b, m);
		if (inCheck(b, !b.turn)) {
			undoMove(b, m, u);
			continue;
		}

		movesSearched++;

		bool isCheck = inCheck(b, b.turn);
		if (isHash) si.hashCount++;

		score = alpha + 1;

		// Step 11a: Late move reduction
		// The moves at the back of the move list are probably unimportant, so we search them at a reduced depth
		if (depth >= lateMoveMinDepth && !isNoisy && !isInCheck && !isCheck && !isPassedPawn && !isDangerousPawn && !isHash && movesSearched > 1) {
			// Step 11b: We obtain base R from a precomputed table
			int lmrIndex = (depth > 12);
			int lateMoveR = lateMoveRTable[lmrIndex][std::min(movesSearched, 63)];

			// Step 11c: Decrease R by 1 if killer move
			if (isKiller) lateMoveR -= 1;

			// Step 11d: Decrease/increase R if move has good/bad history
			// FIXME: Elo loss
			// lateMoveR -= std::max(std::min(historyScore * 2 / historyMax, 2), -2);

			lateMoveR = std::max(0, std::min(lateMoveR, depth - 1));  // Do not drop directly into qsearch
			score = -search(b, depth - lateMoveR - 1, ply + 1, -alpha - 1, -alpha, si, pv);
		}

		// Step 12: Do a complete search if we are searching for the first time/the reduced search from LMR raised alpha
		if (score > alpha) score = -search(b, depth - 1, ply + 1, -beta, -alpha, si, pv);

		undoMove(b, m, u);

		if (score >= beta) {
			storeTT(b.key, depth, beta, TT_BETA, eval, ply, m);

			// Step 13: Killer heuristic
			// Quiet moves that cause a cutoff might be good in the same ply
			// We maintain two buckets each ply
			if (!isNoisy && m != killers[0][ply]) {
				killers[1][ply] = killers[0][ply];
				killers[0][ply] = m;
			}

			// Step 14a: History heuristic
			// Quiet moves that cause lots of cutoffs across different positions are generally good
			// We increment history scores by depth squared in order to increase the importance of cutoffs near the root
			// We limit history scores to a certain maximum depth as they tend to become noise at higher depths
			if (!isNoisy && depth <= historyMaxDepth) {
				updateHistory(historyMoves[b.turn][pieceType(b.squares[from])][to], depth * depth);
			}

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
		} else {
			// Step 14b: History heuristic
			// We give a penalty to quiet moves that did not raise alpha
			if (!isNoisy && depth <= historyMaxDepth) {
				updateHistory(historyMoves[b.turn][pieceType(b.squares[from])][to], -depth * depth / 2);
			}
		}
	}

	// Step 15: Checkmate/stalemate
	// If we haven't searched any legal moves, we are either checkmated or stalemated
	if (!movesSearched) {
		return (isInCheck) ? -MATE_SCORE + ply : 0;
	}

	// Step 16: Store transposition table
	// We store the results of our search in the transposition table
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

	// Step 1: Check for 3-fold repetition
	if (drawnByRepetition(b)) return 0;

	// Step 2: Probe quiescence search evaluation hash table
	// If we have already visited this position, we can reuse the evaluation score without having to calculate it again
	int qHashEval = probeQHash(b.key);
	bool hit = (qHashEval != NO_VALUE);

	int eval = (hit) ? qHashEval : evaluate(b, b.turn);

	if (!hit) storeQHash(b.key, eval);
	if (hit) si.qHashHit++;

	// Step 3: Standing pat
	// If the static evaluation alone is good enough to cause a beta cutoff, we cutoff immediately
	if (eval >= beta) return beta;
	if (eval > alpha) alpha = eval;

	// Step 4: Delta pruning
	// We cutoff immediately if our greatest tactial gain plus a safety margin is still not enough to raise alpha
	if (eval + greatestTacticalGain(b) + deltaMargin < alpha) return eval;

	auto noisyMoves = genNoisyMoves(b);
	if (noisyMoves.empty()) return eval;

	std::vector<ScoredMove> scoredNoisyMoves = scoreNoisyMoves(b, noisyMoves);
	std::sort(scoredNoisyMoves.begin(), scoredNoisyMoves.end(), [](const auto& a, const auto& b) { return a.score > b.score; });

	Move pv[MAX_PLY];
	for (auto& m : pv) m = NO_MOVE;

	int movesSearched = 0;

	for (int i = 0, size = scoredNoisyMoves.size(); i < size; ++i) {
		Move m = scoredNoisyMoves[i].m;

		// Step 5: Delta pruning
		// We skip this move if the gain from making this capture plus a safety margin is still not enough to raise alpha
		if (m.getFlag() < PROMOTION_KNIGHT && eval + deltaMargin + pieceValues[pieceType(b.squares[m.getTo()])][MG] < alpha) continue;

		// Step 6: Negative SEE pruning
		// We skip this move if we would lose the exchange that would ensue
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
	Move bestMove = NO_MOVE;
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
		if (si.debug) si.printSearchDebug();

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