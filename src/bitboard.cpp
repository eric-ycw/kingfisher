#include "bitboard.h"
#include "types.h"

int countBits(uint64_t b) {
	int r;
	for (r = 0; b; ++r, b &= b - 1);
	return r;
}

bool checkBit(const uint64_t& b, int sqr) {
	assert(validSquare(sqr));
	return (b & (1ull << sqr)) > 0;
}

// Matt Taylor's folding trick
int lsb(const uint64_t& b) {
	if (!b) return 0;
	uint64_t bb = b ^ (b - 1);
	uint32_t folded = (uint32_t)((bb & 0xffffffff) ^ (bb >> 32));
	return lsbTable[(folded * 0x78291acf) >> 26];
}

// De Bruijn multiplication
int msb(const uint64_t& b) {
	if (!b) return 0;
	uint64_t bb = b;
	bb |= bb >> 1;
	bb |= bb >> 2;
	bb |= bb >> 4;
	bb |= bb >> 8;
	bb |= bb >> 16;
	bb |= bb >> 32;
	return msbTable[(bb * 0x03f79d71b4cb0a89) >> 58];
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
	std::cout << "\n" << "Population count: " << countBits(b) << "\n";
}