#include <bit>
#include <iostream>
#include "Board.hpp"

/* PRIVATE */

void printBitboard(uint64_t bitboard, int idx = -1) {
    std::cout << "\n";
    for (int rank = 7; rank >= 0; rank--) {
        std::cout << rank + 1 << "  "; // Stampa il numero della traversa (1-8)
        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file;
            if (getBit(bitboard, square)) {
                std::cout << "1 ";
            } else if (square == idx) {
                std::cout << "x ";
            } else {
                std::cout << ". ";
            }
        }
        std::cout << "\n";
    }
    std::cout << "\n   a b c d e f g h\n\n";
}

uint64_t Board::ComputePawnMoves(int color, uint64_t bitboard) {
    uint64_t allMoves = (uint64_t) 0;

    // --- 1. Single push ---
    uint64_t singlePush = (color == Color::WHITE) ? (bitboard << 8) : (bitboard >> 8);
    singlePush &= freeCells; // Only empty cells
    allMoves |= singlePush;  // Save the valid steps

    // --- 2. Double push ---
    // Take the pawns in the third (for white) or sixth (for black) raw and check for a double movement
    uint64_t doublePush = (color == Color::WHITE) ? (singlePush & LookupTables::thirdRow) : (singlePush & LookupTables::sixthRow);
    doublePush = (color == Color::WHITE) ? (doublePush << 8) : (doublePush >> 8);
    doublePush &= freeCells;
    allMoves |= doublePush; 

    // --- 3. Check for capture in parallel ---
    uint64_t enemyCells = GetColorOccupation(color == Color::WHITE ? Color::BLACK : Color::WHITE);
    uint64_t westCapture = color == Color::WHITE ? (bitboard << 7) & LookupTables::notColumnH & enemyCells :
                                            (bitboard >> 9) & LookupTables::notColumnA & enemyCells;
    uint64_t estCapture  = color == Color::WHITE ? (bitboard << 9) & LookupTables::notColumnA & enemyCells :
                                            (bitboard >> 7) & LookupTables::notColumnH & enemyCells;

    // TODO: En passant

    return (allMoves | estCapture) | westCapture;
}

uint64_t Board::ComputeKnightMoves(int color, uint64_t bitboard){
    uint64_t allMoves = (uint64_t) 0;
    uint64_t allies = GetColorOccupation(color);

    while (bitboard != (uint64_t) 0 ) {
        int square = __builtin_ctzll(bitboard);        // Funzione intrinseca che trova l'LSB
        uint64_t attacks = LookupTables::knightAttacks[square] & (~allies);
        allMoves |= attacks;
        bitboard = clearBit(bitboard, square);         // To avoid an infinite loop, remove the knight
    }
    return allMoves;
}



/* PUBLIC  */

void Board::InitializeBoard(){
    for (int i = 0; i < NUM_PIECES; i++){
        int shiftW = i == PiecesEnum::PAWNS ? 8 : 0;
        int shiftB = i == PiecesEnum::PAWNS ? 48 : 56;
        sides[Color::WHITE][i] = (uint64_t) GetBinValue(i) << shiftW;
        sides[Color::BLACK][i] = (uint64_t) GetBinValue(i) << shiftB;

        colorOccupation[Color::WHITE] |= sides[Color::WHITE][i];
        colorOccupation[Color::BLACK] |= sides[Color::BLACK][i];
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

uint64_t Board::GetGeneratedMoves(int color, uint64_t bitboard, PiecesEnum piece){
    if (piece == PiecesEnum::PAWNS) return ComputePawnMoves(color, bitboard);
    else if (piece == PiecesEnum::KNIGHTS) return ComputeKnightMoves(color, bitboard);
    
    return 0;
}