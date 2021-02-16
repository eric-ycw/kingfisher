#ifndef MASKS_H
#define MASKS_H

#include "types.h"
#include "bitboard.h"

extern uint64_t pawnAdvanceMasks[8][2];
extern uint64_t neighborFileMasks[8];
extern uint64_t passedPawnMasks[SQUARE_NUM][2];
extern uint64_t kingInnerRing[SQUARE_NUM];

void initPawnAdvanceMasks();
void initNeighborFileMasks();
void initPassedPawnMasks();

void initKingInnerRing();

void initMasks();

#endif
