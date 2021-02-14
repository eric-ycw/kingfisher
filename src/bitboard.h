#ifndef BITBOARD_H
#define BITBOARD_H

#include "types.h"

static constexpr uint64_t rank1Mask = 0x00000000000000FFull;
static constexpr uint64_t rank2Mask = 0x000000000000FF00ull;
static constexpr uint64_t rank3Mask = 0x0000000000FF0000ull;
static constexpr uint64_t rank4Mask = 0x00000000FF000000ull;
static constexpr uint64_t rank5Mask = 0x000000FF00000000ull;
static constexpr uint64_t rank6Mask = 0x0000FF0000000000ull;
static constexpr uint64_t rank7Mask = 0x00FF000000000000ull;
static constexpr uint64_t rank8Mask = 0xFF00000000000000ull;

static constexpr uint64_t fileAMask = 0x0101010101010101ull;
static constexpr uint64_t fileBMask = 0x0202020202020202ull;
static constexpr uint64_t fileCMask = 0x0404040404040404ull;
static constexpr uint64_t fileDMask = 0x0808080808080808ull;
static constexpr uint64_t fileEMask = 0x1010101010101010ull;
static constexpr uint64_t fileFMask = 0x2020202020202020ull;
static constexpr uint64_t fileGMask = 0x4040404040404040ull;
static constexpr uint64_t fileHMask = 0x8080808080808080ull;

static constexpr uint64_t rankMasks[8] = {
	rank1Mask, rank2Mask, rank3Mask, rank4Mask, rank5Mask, rank6Mask, rank7Mask, rank8Mask
};
static constexpr uint64_t fileMasks[8] = {
	fileAMask, fileBMask, fileCMask, fileDMask, fileEMask, fileFMask, fileGMask, fileHMask
};

static constexpr int msbTable[SQUARE_NUM] = {
	0, 47,  1, 56, 48, 27,  2, 60,
   57, 49, 41, 37, 28, 16,  3, 61,
   54, 58, 35, 52, 50, 42, 21, 44,
   38, 32, 29, 23, 17, 11,  4, 62,
   46, 55, 26, 59, 40, 36, 15, 53,
   34, 51, 20, 43, 31, 22, 10, 45,
   25, 39, 14, 33, 19, 30,  9, 24,
   13, 18,  8, 12,  7,  6,  5, 63
};

int countBits(uint64_t b);
bool checkBit(const uint64_t& b, int sqr);
int lsb(const uint64_t& b);
int msb(const uint64_t& b);
int generalBitscan(const uint64_t& b, const int dir);
int popBit(uint64_t& b);
void setBit(uint64_t& b, int sqr);
void setBitIfValid(uint64_t& b, int sqr);
void clearBit(uint64_t& b, int sqr);

void printBitboard(const uint64_t& b);

#endif