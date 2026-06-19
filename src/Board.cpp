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
    }
    
    UpdateGlobalBoardState();
}

uint64_t Board::GetGeneratedMoves(int color, uint64_t bitboard, PiecesEnum::Type piece){
    switch (piece) {
        case PiecesEnum::PAWNS:   return ComputePawnMoves(color, bitboard);
        case PiecesEnum::KNIGHTS: return ComputeKnightMoves(color, bitboard);
        case PiecesEnum::KING:    return ComputeKingMoves(color, bitboard);
        case PiecesEnum::BISHOPS: return ComputeBishopMoves(color, bitboard);
        case PiecesEnum::ROOKS:   return ComputeRookMoves(color, bitboard);
        case PiecesEnum::QUEEN:   return ComputeQueenMoves(color, bitboard);
        case PiecesEnum::NONE:    return -1;
    }
    return 0;
}

bool Board::MakeMove(Move move){

    int from = getMoveFrom(move);
    int to = getMoveTo(move);
    int flags = getMoveFlags(move);

    int us = sideToMove;
    int them = us == Color::WHITE ? Color::BLACK : Color::WHITE;

    // Find correct bitboard to modify
    PiecesEnum::Type pieceType;
    for ( const auto piece : PiecesEnum::All ){
        if (getBit(sides[us][piece], from)) {
            pieceType = piece;
            break;
        }
    }

    // Captures
    bool captured = false;
    PiecesEnum::Type capturedPieceType = PiecesEnum::NONE;
    if (flags == FlagMap::CAPTURE || flags == FlagMap::ENPASS || flags == FlagMap::PRCAPBISHOP ||
        flags == FlagMap::PRCAPKNIGHT || flags == FlagMap::PRCAPQUEEN || flags == FlagMap::PRCAPROOK) {
        captured = true;
        for ( const auto piece : PiecesEnum::All ){
            if (getBit(sides[them][piece], to)){
                capturedPieceType = piece;
                break;
            }
        }
    }

    history[historyPly].movedPiece = pieceType;
    history[historyPly].capturedPiece = capturedPieceType;
    historyPly++;

    // Move our piece
    sides[us][pieceType] = clearBit(sides[us][pieceType], from);
    sides[us][pieceType] = setBit(sides[us][pieceType], to);
    // Delete the enemy piece
    if (captured){
        sides[them][capturedPieceType] = clearBit(sides[them][capturedPieceType], to);
    }

    UpdateGlobalBoardState();

    uint64_t kingPosition = __builtin_ctzll(sides[us][PiecesEnum::KING]);
    if (IsSquareAttacked(kingPosition, them)){
        UnmakeMove(move);
        return false;
    }

    // Change turn
    sideToMove = them;
    return true;
}

bool Board::IsSquareAttacked(int square, int attackingColor) {
    int us = sideToMove; // Il colore che si sta difendendo (es. il Re sotto controllo)

    uint64_t pawnTriggers = LookupTables::pawnAttacks[us][square];
    if (pawnTriggers & sides[attackingColor][PiecesEnum::PAWNS]) return true;

    uint64_t knightTriggers = LookupTables::knightAttacks[square];
    if (knightTriggers & sides[attackingColor][PiecesEnum::KNIGHTS]) return true;

    uint64_t kingTriggers = LookupTables::kingAttacks[square];
    if (kingTriggers & sides[attackingColor][PiecesEnum::KING]) return true;

    uint64_t bishopTriggers = (uint64_t)0;
    bishopTriggers |= ComputeRay(9, LookupTables::notColumnA,  square);
    bishopTriggers |= ComputeRay(7, LookupTables::notColumnH,  square);
    bishopTriggers |= ComputeRay(-9, LookupTables::notColumnH,  square);
    bishopTriggers |= ComputeRay(-7, LookupTables::notColumnA,  square);
    
    uint64_t diagonalThreaths = sides[attackingColor][PiecesEnum::BISHOPS] | sides[attackingColor][PiecesEnum::QUEEN];
    if (bishopTriggers & diagonalThreaths) return true;

    uint64_t rookTriggers = (uint64_t)0;
    rookTriggers |= ComputeRay(8, ~0ULL,  square);
    rookTriggers |= ComputeRay(1, LookupTables::notColumnA,  square);
    rookTriggers |= ComputeRay(-8, ~0ULL,  square);
    rookTriggers |= ComputeRay(-1, LookupTables::notColumnH,  square);

    uint64_t orthogonalThreaths = sides[attackingColor][PiecesEnum::ROOKS] | sides[attackingColor][PiecesEnum::QUEEN];
    if (rookTriggers & orthogonalThreaths) return true;

    return false;
}

void Board::UnmakeMove(Move move) {
    int to = getMoveTo(move);
    int from = getMoveFrom(move);

    int us = Color::WHITE;
    if (getBit(colorOccupation[Color::BLACK], to)) {
        us = Color::BLACK;
    }
    int them = (us == Color::WHITE) ? Color::BLACK : Color::WHITE;

    historyPly--;
    PiecesEnum::Type pieceType = history[historyPly].movedPiece;
    PiecesEnum::Type capturedPiece = history[historyPly].capturedPiece;

    sides[us][pieceType] = setBit(sides[us][pieceType], from);
    sides[us][pieceType] = clearBit(sides[us][pieceType], to);
    
    if (capturedPiece != PiecesEnum::NONE) {
        sides[them][capturedPiece] = setBit(sides[them][capturedPiece], to);
    }

    sideToMove = us;
    UpdateGlobalBoardState();
}

void Board::UpdateGlobalBoardState() {
    for (int color = 0; color < 2; color++){
        colorOccupation[color] = 0;
        for (const auto piece : PiecesEnum::All) {
            colorOccupation[color] |= sides[color][piece];
        }
    }
    totalOccupation = colorOccupation[0] | colorOccupation[1];
    freeCells = ~totalOccupation;  
}

int Board::Evaluate() {
    int whiteMaterial = 0;
    int blackMaterial = 0;

    // Standard poits (centipawns): P=100, C=300, A=300, T=500, D=900
    whiteMaterial += __builtin_popcountll(sides[Color::WHITE][PiecesEnum::PAWNS]) * 100;
    whiteMaterial += __builtin_popcountll(sides[Color::WHITE][PiecesEnum::KNIGHTS]) * 300;
    whiteMaterial += __builtin_popcountll(sides[Color::WHITE][PiecesEnum::BISHOPS]) * 300;
    whiteMaterial += __builtin_popcountll(sides[Color::WHITE][PiecesEnum::ROOKS]) * 500;
    whiteMaterial += __builtin_popcountll(sides[Color::WHITE][PiecesEnum::QUEEN]) * 900;

    blackMaterial += __builtin_popcountll(sides[Color::BLACK][PiecesEnum::PAWNS]) * 100;
    blackMaterial += __builtin_popcountll(sides[Color::BLACK][PiecesEnum::KNIGHTS]) * 300;
    blackMaterial += __builtin_popcountll(sides[Color::BLACK][PiecesEnum::BISHOPS]) * 300;
    blackMaterial += __builtin_popcountll(sides[Color::BLACK][PiecesEnum::ROOKS]) * 500;
    blackMaterial += __builtin_popcountll(sides[Color::BLACK][PiecesEnum::QUEEN]) * 900;

    // If white is winning, evaluation > 0 else negative
    int evaluation = whiteMaterial - blackMaterial;

    // "NEGAMAX"
    return (sideToMove == Color::WHITE) ? evaluation : -evaluation;
}