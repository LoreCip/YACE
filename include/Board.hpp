#ifndef _BOARD
#define _BOARD

#include <cstdint>
#include "Pieces.hpp"
#include "BitOperations.hpp"

class Board {
private:
    uint64_t sides[2][NUM_PIECES] = {(uint64_t)0};
    uint64_t colorOccupation[2] = {(uint64_t)0};
    uint64_t totalOccupation{(uint64_t)0};
    uint64_t freeCells{(uint64_t)0};

public:
    void InitializeBoard();
    uint64_t GetColorOccupation(int color);
    uint64_t GetTotalOccupation();
    uint64_t GetFreeCells();
};

#endif