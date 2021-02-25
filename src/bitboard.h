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

static constexpr uint64_t bordersMask = fileAMask | fileHMask | rank1Mask | rank8Mask;
static constexpr uint64_t cornersMask = (1ull << A1) | (1ull << A8) | (1ull << H1) | (1ull << H8);
static constexpr uint64_t middleFileMask = (fileCMask | fileDMask | fileEMask | fileFMask);
static constexpr uint64_t centerMasks[2] = { middleFileMask & (rank2Mask | rank3Mask | rank4Mask),
                                             middleFileMask & (rank5Mask | rank6Mask | rank7Mask) };

int countBits(const uint64_t& b);
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