#include <cstdint>
#include <sys/types.h>

#include "Board.hpp"
#include "BitOperations.hpp"
#include "LookupTables.hpp"
#include "Move.hpp"
#include "Pieces.hpp"

/* PRIVATE */

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

uint64_t Board::ComputeKingMoves(int color, uint64_t bitboard){
    uint64_t allMoves = (uint64_t) 0;
    uint64_t allies = GetColorOccupation(color);

    while (bitboard != (uint64_t) 0 ) {
        int square = __builtin_ctzll(bitboard);        // Funzione intrinseca che trova l'LSB
        uint64_t attacks = LookupTables::kingAttacks[square] & (~allies);
        allMoves |= attacks;
        bitboard = clearBit(bitboard, square);         // To avoid an infinite loop, remove the knight
    }

    // TODO: Arrocco

    return allMoves;
}

uint64_t Board::ComputeBishopMoves(int color, uint64_t bitboard){
    uint64_t allMoves = (uint64_t) 0;
    uint64_t allies = GetColorOccupation(color);

    while (bitboard != (uint64_t) 0 ) {
        int square = __builtin_ctzll(bitboard);         // Funzione intrinseca che trova l'LSB
        allMoves |= ComputeRay(9, LookupTables::notColumnA,  square);
        allMoves |= ComputeRay(7, LookupTables::notColumnH,  square);
        allMoves |= ComputeRay(-9, LookupTables::notColumnH,  square);
        allMoves |= ComputeRay(-7, LookupTables::notColumnA,  square);

        bitboard = clearBit(bitboard, square); // To avoid an infinite loop, remove the bishop
    }

    return allMoves & (~allies);
}

uint64_t Board::ComputeRookMoves(int color, uint64_t bitboard){
    uint64_t allMoves = (uint64_t) 0;
    uint64_t allies = GetColorOccupation(color);

    while (bitboard != (uint64_t) 0 ) {
        int square = __builtin_ctzll(bitboard);         // Funzione intrinseca che trova l'LSB
        allMoves |= ComputeRay(-1, LookupTables::notColumnH,  square);
        allMoves |= ComputeRay(1, LookupTables::notColumnA,  square);
        allMoves |= ComputeRay(8, LookupTables::nullEdge,  square);
        allMoves |= ComputeRay(-8, LookupTables::nullEdge,  square);

        bitboard = clearBit(bitboard, square); // To avoid an infinite loop, remove the rook
    }

    return allMoves & (~allies);
}

uint64_t Board::ComputeQueenMoves(int color, uint64_t bitboard){
    uint64_t allMoves = (uint64_t) 0;
    uint64_t allies = GetColorOccupation(color);

    while (bitboard != (uint64_t) 0 ) { // generally only one loop, but there might be more than one
        int square = __builtin_ctzll(bitboard);         // Funzione intrinseca che trova l'LSB
        allMoves |= ComputeRay(8, LookupTables::nullEdge,  square);
        allMoves |= ComputeRay(1, LookupTables::notColumnA,  square);
        allMoves |= ComputeRay(-8, LookupTables::nullEdge,  square);
        allMoves |= ComputeRay(-1, LookupTables::notColumnH,  square);
        allMoves |= ComputeRay(9, LookupTables::notColumnH,  square);
        allMoves |= ComputeRay(7, LookupTables::notColumnA,  square);
        allMoves |= ComputeRay(-9, LookupTables::notColumnA,  square);
        allMoves |= ComputeRay(-7, LookupTables::notColumnH,  square);

        bitboard = clearBit(bitboard, square); // To avoid an infinite loop, remove the queen
    }

    return allMoves & (~allies);
}

uint64_t Board::ComputeRay(int direction, uint64_t edge, int square){
    uint64_t allMoves = (uint64_t) 0;
    uint64_t ray = (uint64_t)1 << square;
    while(true) {
        // 1. Applichi la maschera di bordo PRIMA dello shift per bloccare il bit se è sul confine
        ray &= edge; 
        if (ray == (uint64_t) 0) break;

        // 2. Esegui lo shift in sicurezza
        ray = direction > 0 ? ray << direction : ray >> (-direction);
        if (ray == (uint64_t) 0) break;

        // 3. Salvi la mossa e verifichi gli ostacoli occupazionali
        allMoves |= ray;
        if (ray & totalOccupation) break;
    }
    return allMoves;
}


/* PUBLIC  */

void Board::InitializeBoard(){
    
    // White always starts
    sideToMove = Color::WHITE; 

    // Fill board
    for (int color = 0; color < 2; color++){
       
        // Pawns
        int startPos = color == Color::WHITE ? 8 : 48;
        for (int nPawn = 0; nPawn < 8; nPawn++){
            sides[color][PiecesEnum::PAWNS] = setBit(sides[color][PiecesEnum::PAWNS], startPos+nPawn);
        }

        startPos = color == Color::WHITE ? 0 : 56;
        // Rooks
        sides[color][PiecesEnum::ROOKS] = setBit(sides[color][PiecesEnum::ROOKS], startPos);
        sides[color][PiecesEnum::ROOKS] = setBit(sides[color][PiecesEnum::ROOKS], startPos+7);
        // Knights
        sides[color][PiecesEnum::KNIGHTS] = setBit(sides[color][PiecesEnum::KNIGHTS], startPos+1);
        sides[color][PiecesEnum::KNIGHTS] = setBit(sides[color][PiecesEnum::KNIGHTS], startPos+6);
        // Bishops
        sides[color][PiecesEnum::BISHOPS] = setBit(sides[color][PiecesEnum::BISHOPS], startPos+2);
        sides[color][PiecesEnum::BISHOPS] = setBit(sides[color][PiecesEnum::BISHOPS], startPos+5);
        // Queen
        sides[color][PiecesEnum::QUEEN] = setBit(sides[color][PiecesEnum::QUEEN], startPos+3);
        // Queen
        sides[color][PiecesEnum::KING] = setBit(sides[color][PiecesEnum::KING], startPos+4);

        for (int i = 0; i < NUM_PIECES; i++) {
            colorOccupation[color] |= sides[color][i];    
        }
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
    else if (piece == PiecesEnum::KING) return ComputeKingMoves(color, bitboard);
    else if (piece == PiecesEnum::BISHOPS) return ComputeBishopMoves(color, bitboard);
    else if (piece == PiecesEnum::ROOKS) return ComputeRookMoves(color, bitboard);
    else if (piece == PiecesEnum::QUEEN) return ComputeQueenMoves(color, bitboard);
    else return 0;
}

bool Board::MakeMove(Move move){
    int from = getMoveFrom(move);
    int to = getMoveTo(move);
    int flags = getMoveFlags(move);

    int us = sideToMove;
    int them = us == Color::WHITE ? Color::BLACK : Color::WHITE;

    // Find correct bitboard to modify
    int pieceType;
    for (pieceType = 0; pieceType < NUM_PIECES; pieceType++){
        if (getBit(sides[us][pieceType], from)) break;
    }

    // Captures
    int capturedPieceType = -1;
    if (flags == FlagMap::CAPTURE || flags == FlagMap::ENPASS || flags == FlagMap::PRCAPBISHOP ||
        flags == FlagMap::PRCAPKNIGHT || flags == FlagMap::PRCAPQUEEN || flags == FlagMap::PRCAPROOK) { 
        for (capturedPieceType = 0; capturedPieceType < NUM_PIECES; capturedPieceType++){
            if (getBit(sides[them][capturedPieceType], to)) break;
        }
    }

    // Move our piece
    sides[us][pieceType] = clearBit(sides[us][pieceType], from);
    sides[us][pieceType] = setBit(sides[us][pieceType], to);
    // Eventually delete the enemies piece
    sides[them][capturedPieceType] = flags & 1 ? clearBit(sides[them][capturedPieceType], to) :
                                                     sides[them][capturedPieceType];
    
    // Update global board
    for (int color = 0; color < 2; color++){
        for (int i = 0; i < NUM_PIECES; i++) {
            colorOccupation[color] |= sides[color][i];
        }
    }
    totalOccupation = colorOccupation[0] | colorOccupation[1];
    freeCells = ~totalOccupation;  

    // Change turn
    sideToMove = them;

    
    return true;
}


void Board::UnmakeMove(Move move){
    return;
}