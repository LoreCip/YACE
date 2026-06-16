#include <cstdint>

uint64_t setBit(uint64_t bitboard, int position){
    uint64_t mask = (uint64_t)1 << position;
    return bitboard | mask;
}

uint64_t clearBit(uint64_t bitboard, int position){
    uint64_t mask = ~((uint64_t)1 << position);
    return bitboard & mask;
}

uint64_t getBit(uint64_t bitboard, int position){
    uint64_t mask = (uint64_t)1 << position;
    return (bitboard & mask) != 0;
}

uint64_t toggleBit(uint64_t bitboard, int position){
    uint64_t mask = (uint64_t)1 << position;
    return bitboard ^ mask;
}