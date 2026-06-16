#include "Board.hpp"

void Board::InitializeBoard(){
    for (int i = 0; i < NUM_PIECES; i++){
        int shiftW = i == PiecesEnum::PAWNS ? 8 : 0;
        int shiftB = i == PiecesEnum::PAWNS ? 48 : 56;
        sides[0][i] = (uint64_t) GetBinValue(i) << shiftW;
        sides[1][i] = (uint64_t) GetBinValue(i) << shiftB;

        colorOccupation[0] |= sides[0][i];
        colorOccupation[1] |= sides[1][i];
    }

    totalOccupation = colorOccupation[0] | colorOccupation[1];
    freeCells = ~totalOccupation;  
}

uint64_t Board::GetColorOccupation(int color){
    return colorOccupation[color];
}

uint64_t Board::GetTotalOccupation(){
    return totalOccupation;
}

uint64_t Board::GetFreeCells(){
    return freeCells;
}