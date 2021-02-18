#include "attacks.h"
#include "bitboard.h"
#include "masks.h"
#include "types.h"

uint64_t bishopBlockerMasks[SQUARE_NUM];
uint64_t rookBlockerMasks[SQUARE_NUM];

uint64_t pawnAdvanceMasks[8][2];
uint64_t neighborFileMasks[8];
uint64_t passedPawnMasks[SQUARE_NUM][2];

uint64_t kingRing[SQUARE_NUM];

uint64_t bishopMoves[5248];
uint64_t rookMoves[102400];

void initBishopBlockerMasks() {
    for (int sqr = 0; sqr < SQUARE_NUM; ++sqr) {
        uint64_t blockers = 0;
        for (int i = 0; i < 4; ++i) {
            blockers |= bishopAttacks[sqr][i];
        }
        // For bishops, removing all borders removes the last bit of every ray
        bishopBlockerMasks[sqr] = blockers & ~bordersMask;
    }
}

void initRookBlockerMasks() {
    for (int sqr = 0; sqr < SQUARE_NUM; ++sqr) {
        uint64_t blockers = 0;
        for (int i = 0; i < 4; ++i) {
            blockers |= rookAttacks[sqr][i];
        }
        if (checkBit(bordersMask, sqr)) {
            // The rook is on a border square
            // We remove corners
            blockers &= ~cornersMask;
            // We remove all borders except the one(s) the rook is on
            uint64_t occBorders = 0;
            occBorders |= (((1ull << sqr) & fileAMask) > 0) ? fileAMask : 0;
            occBorders |= (((1ull << sqr) & fileHMask) > 0) ? fileHMask : 0;
            occBorders |= (((1ull << sqr) & rank1Mask) > 0) ? rank1Mask : 0;
            occBorders |= (((1ull << sqr) & rank8Mask) > 0) ? rank8Mask : 0;
            blockers &= ~(bordersMask ^ occBorders);

        } else {
            // For rooks not on border squares, removing all borders removes the last bit of every ray
            blockers &= ~bordersMask;
        }
        rookBlockerMasks[sqr] = blockers;
    }
}

void initBishopMagics() {
    for (int sqr = 0; sqr < SQUARE_NUM; ++sqr) {
        // We use the Carry-Rippler trick to enumerate through all possible permutations of blockers
        // For more information, see https://www.chessprogramming.org/Traversing_Subsets_of_a_Set#All_Subsets_of_any_Set
        uint64_t n = 0;
        do {
            *(((n * bishopMagics[sqr]) >> bishopMagicShifts[sqr]) + bishopMagicIndexIncrements[sqr]) = getBishopAttacks(n, sqr);
            n = (n - bishopBlockerMasks[sqr]) & bishopBlockerMasks[sqr];
        } while (n);
    }
}

void initRookMagics() {
    for (int sqr = 0; sqr < SQUARE_NUM; ++sqr) {
        // We use the Carry-Rippler trick to enumerate through all possible permutations of blockers
        // For more information, see https://www.chessprogramming.org/Traversing_Subsets_of_a_Set#All_Subsets_of_any_Set
        uint64_t n = 0;
        do {
            *(((n * rookMagics[sqr]) >> rookMagicShifts[sqr]) + rookMagicIndexIncrements[sqr]) = getRookAttacks(n, sqr);
            n = (n - rookBlockerMasks[sqr]) & rookBlockerMasks[sqr];
        } while (n);
    }
}

void initPawnAdvanceMasks() {
    for (int i = 0; i < 8; ++i) {
        uint64_t wMask = 0ull;
        for (int j = 6; j > i; --j) {
            wMask |= rankMasks[j];
        }
        pawnAdvanceMasks[i][WHITE] = wMask;

        uint64_t bMask = 0ull;
        for (int j = 1; j < i; ++j) {
            bMask |= rankMasks[j];
        }
        pawnAdvanceMasks[i][BLACK] = bMask;
    }
}

void initNeighborFileMasks() {
    for (int i = 0; i < 8; ++i) {
        neighborFileMasks[i] = fileMasks[i] | fileMasks[std::max(0, i - 1)] | fileMasks[std::min(7, i + 1)];
    }
}

void initPassedPawnMasks() {
    for (int sqr = 0; sqr < SQUARE_NUM; ++sqr) {
        int rank = sqr / 8;
	    int file = sqr % 8;
        passedPawnMasks[sqr][WHITE] = pawnAdvanceMasks[rank][WHITE] & neighborFileMasks[file];
        passedPawnMasks[sqr][BLACK] = pawnAdvanceMasks[rank][BLACK] & neighborFileMasks[file];
    }
}

void initKingRing() {
    for (int sqr = 0; sqr < SQUARE_NUM; ++ sqr) {
        uint64_t kingSquare = (1ull << sqr);

        // FIXME: Expanding king ring to a full 9 squares at borders and corners somehow leads to elo loss
        /*
        if (kingSquare & fileAMask) {
            if (kingSquare & rank1Mask) { // A1
                kingSquare = (1ull << B2);
            } else if (kingSquare & rank8Mask) { // A8
                kingSquare = (1ull << B7);
            } else {
                kingSquare = (1ull << (sqr + 1));
            }
        } else if (kingSquare & fileHMask) {
             if (kingSquare & rank1Mask) { // H1
                kingSquare = (1ull << G2);
            } else if (kingSquare & rank8Mask) { // H8
                kingSquare = (1ull << G7);
            } else {
                kingSquare = (1ull << (sqr - 1));
            }
        } else if (kingSquare & rank1Mask) { // Except corners
            kingSquare = (1ull << (sqr + 8));
        } else if (kingSquare & rank8Mask) { // Except corners
            kingSquare = (1ull << (sqr - 8));
        }*/

	    uint64_t diagonals = ((kingSquare >> 7) & ~fileAMask) | ((kingSquare >> 9) & ~fileHMask) | ((kingSquare << 7) & ~fileHMask) | ((kingSquare << 9) & ~fileAMask);
	    uint64_t cardinals = ((kingSquare >> 1) & ~fileHMask) | ((kingSquare << 1) & ~fileAMask) | (kingSquare >> 8) | (kingSquare << 8);
	    kingRing[sqr] = kingSquare | diagonals | cardinals;
    }
}

void initMasks() {
    initBishopBlockerMasks();
    initRookBlockerMasks();

    initPawnAdvanceMasks();
    initNeighborFileMasks();
    initPassedPawnMasks();

    initKingRing();

    initBishopMagics();
    initRookMagics();
}