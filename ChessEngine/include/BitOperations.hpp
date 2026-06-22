#ifndef _BITOPERATIONS
#define _BITOPERATIONS

#include <cstdint>

uint64_t setBit(uint64_t bitboard, int position);
uint64_t clearBit(uint64_t bitboard, int position);
uint64_t getBit(uint64_t bitboard, int position);
uint64_t toggleBit(uint64_t bitboard, int position);

#endif