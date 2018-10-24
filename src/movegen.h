#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "types.h"

void addPawnMoves(std::vector<Move>& moves, uint64_t bb, const int shift);
void addPieceMoves(std::vector<Move>& moves, uint64_t bb, const int from);

void genPawnMoves(const Board& b, std::vector<Move>& moves, bool noisyOnly);
void genKnightMoves(const Board& b, std::vector<Move>& moves, bool noisyOnly);
void genBishopMoves(const Board& b, std::vector<Move>& moves, bool noisyOnly);
void genRookMoves(const Board& b, std::vector<Move>& moves, bool noisyOnly);
void genQueenMoves(const Board& b, std::vector<Move>& moves, bool noisyOnly);
void genKingMoves(const Board& b, std::vector<Move>& moves, bool noisyOnly);

std::vector<Move> genAllMoves(const Board& b);
std::vector<Move> genNoisyMoves(const Board& b);

#endif
