#include "bitboard.h"
#include "types.h"

int countBits(const uint64_t& b) {
	return __builtin_popcountll(b);
}

bool checkBit(const uint64_t& b, int sqr) {
	assert(validSquare(sqr));
	return (b & (1ull << sqr)) > 0;
}

int lsb(const uint64_t& b) {
	return __builtin_ctzll(b);
}

int msb(const uint64_t& b) {
	return __builtin_clzll(b) ^ 63;
}

int generalBitscan(const uint64_t& b, const int dir) {
	return (dir >= 0) ? lsb(b) : msb(b);
}

int popBit(uint64_t& b) {
	int index = lsb(b);
	b &= b - 1;
	return index;
}

void setBit(uint64_t& b, int sqr) {
	assert(!checkBit(b, sqr));
	b ^= (1ull << sqr);
}

void setBitIfValid(uint64_t& b, int sqr) {
	if (validSquare(sqr)) b ^= (1ull << sqr);
}

void clearBit(uint64_t& b, int sqr) {
	assert(checkBit(b, sqr));
	b ^= (1ull << sqr);
}

void printBitboard(const uint64_t& b) {
	for (int row = 7; row >= 0; --row) {
		char l[] = "* * * * * * * * ";
		for (int col = 0; col < 8; ++col) {
			if (checkBit(b, toSquare(row, col))) l[col * 2] = 'X';
		}
		std::cout << l << "\n";
	}
	std::cout << "\n" << "Population count: " << countBits(b) << "\n\n";
}