#ifndef BITOPERATIONS_HPP
#define BITOPERATIONS_HPP

#include <cstdint>

inline uint64_t setBit(uint64_t bitboard, int position){
    uint64_t mask = (uint64_t)1 << position;
    return bitboard | mask;
}

inline uint64_t clearBit(uint64_t bitboard, int position){
    uint64_t mask = ~((uint64_t)1 << position);
    return bitboard & mask;
}

inline bool getBit(uint64_t bitboard, int position){
    return (bitboard >> position) & 1;
}

#endif