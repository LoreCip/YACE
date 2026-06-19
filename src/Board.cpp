#include <cstdint>
#include <sys/types.h>
#include <algorithm>

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
    uint64_t westCapture = color == Color::WHITE ? (bitboard << 7) & LookupTables::notColumnA & enemyCells :
                                            (bitboard >> 9) & LookupTables::notColumnA & enemyCells;
    uint64_t estCapture  = color == Color::WHITE ? (bitboard << 9) & LookupTables::notColumnH & enemyCells :
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
        int square = __builtin_ctzll(bitboard);         
        allMoves |= ComputeRay(9, LookupTables::notColumnH,  square); // NE (+9) -> Stop H
        allMoves |= ComputeRay(7, LookupTables::notColumnA,  square); // NW (+7) -> Stop A
        allMoves |= ComputeRay(-9, LookupTables::notColumnA,  square); // SW (-9) -> Stop A
        allMoves |= ComputeRay(-7, LookupTables::notColumnH,  square); // SE (-7) -> Stop H
        bitboard = clearBit(bitboard, square); 
    }
    return allMoves & (~allies);
}

uint64_t Board::ComputeRookMoves(int color, uint64_t bitboard){
    uint64_t allMoves = (uint64_t) 0;
    uint64_t allies = GetColorOccupation(color);

    while (bitboard != (uint64_t) 0 ) {
        int square = __builtin_ctzll(bitboard);         
        allMoves |= ComputeRay(-1, LookupTables::notColumnA,  square); // Ovest (-1) -> Stop A
        allMoves |= ComputeRay(1, LookupTables::notColumnH,  square);  // Est (+1) -> Stop H
        allMoves |= ComputeRay(8, LookupTables::nullEdge,  square);
        allMoves |= ComputeRay(-8, LookupTables::nullEdge,  square);
        bitboard = clearBit(bitboard, square); 
    }
    return allMoves & (~allies);
}

uint64_t Board::ComputeQueenMoves(int color, uint64_t bitboard){
    uint64_t allMoves = (uint64_t) 0;
    uint64_t allies = GetColorOccupation(color);

    while (bitboard != (uint64_t) 0 ) { 
        int square = __builtin_ctzll(bitboard);         
        allMoves |= ComputeRay(8, LookupTables::nullEdge,  square);
        allMoves |= ComputeRay(1, LookupTables::notColumnH,  square);  // Est -> Stop H
        allMoves |= ComputeRay(-8, LookupTables::nullEdge,  square);
        allMoves |= ComputeRay(-1, LookupTables::notColumnA,  square); // Ovest -> Stop A
        allMoves |= ComputeRay(9, LookupTables::notColumnH,  square);  // NE -> Stop H
        allMoves |= ComputeRay(7, LookupTables::notColumnA,  square);  // NW -> Stop A
        allMoves |= ComputeRay(-9, LookupTables::notColumnA,  square); // SW -> Stop A
        allMoves |= ComputeRay(-7, LookupTables::notColumnH,  square); // SE -> Stop H
        bitboard = clearBit(bitboard, square); 
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

    positionHistory[historyPly] = GetHash();
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
    bishopTriggers |= ComputeRay(9, LookupTables::notColumnH,  square);
    bishopTriggers |= ComputeRay(7, LookupTables::notColumnA,  square);
    bishopTriggers |= ComputeRay(-9, LookupTables::notColumnA,  square);
    bishopTriggers |= ComputeRay(-7, LookupTables::notColumnH,  square);
    
    uint64_t diagonalThreaths = sides[attackingColor][PiecesEnum::BISHOPS] | sides[attackingColor][PiecesEnum::QUEEN];
    if (bishopTriggers & diagonalThreaths) return true;

    uint64_t rookTriggers = (uint64_t)0;
    rookTriggers |= ComputeRay(8, ~0ULL,  square);
    rookTriggers |= ComputeRay(1, LookupTables::notColumnH,  square);
    rookTriggers |= ComputeRay(-8, ~0ULL,  square);
    rookTriggers |= ComputeRay(-1, LookupTables::notColumnA,  square);

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

// Esempio di Piece-Square Table per i Cavalli (incentiva il centro)
const int knightPST[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50
};

// Esempio di PST per i pedoni (incentiva l'avanzamento, lato Bianco)
const int pawnPST[64] = {
      0,  0,  0,  0,  0,  0,  0,  0,
     50, 50, 50, 50, 50, 50, 50, 50,
     10, 10, 20, 30, 30, 20, 10, 10,
      5,  5, 10, 25, 25, 10,  5,  5,
      0,  0,  0, 20, 20,  0,  0,  0,
      5, -5,-10,  0,  0,-10, -5,  5,
      5, 10, 10,-20,-20, 10, 10,  5,
      0,  0,  0,  0,  0,  0,  0,  0
};

// PST per gli Alfieri: incentiva le diagonali attive, il controllo del centro e penalizza i bordi/angoli
const int bishopPST[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20
};

// PST per le Torri: incentiva la permanenza sulle colonne centrali (d, e) e premia moltissimo l'invasione della 7ª traversa
const int rookPST[64] = {
      0,  0,  0,  5,  5,  0,  0,  0,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
     -5,  0,  0,  0,  0,  0,  0, -5,
      5, 10, 10, 10, 10, 10, 10,  5,
      0,  0,  0,  0,  0,  0,  0,  0
};

// PST per la Donna: incentiva la centralizzazione prudente, scoraggia l'uscita prematura sui bordi in apertura
const int queenPST[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -10,  5,  5,  5,  5,  5,  0,-10,
     -5,  0,  5,  5,  5,  5,  0, -5,
      0,  0,  5,  5,  5,  5,  0, -5,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

// PST per il Re (Medio Gioco): punisce severamente il Re al centro o esposto, premia l'arrocco (case g1/c1)
const int kingMiddlePST[64] = {
     20, 30, 10,  0,  0, 10, 30, 20,
     20, 20,  0,  0,  0,  0, 20, 20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30
};

int Board::Evaluate() {
    int whiteScore = 0;
    int blackScore = 0;

    whiteScore += __builtin_popcountll(sides[Color::WHITE][PiecesEnum::PAWNS]) * 100;
    whiteScore += __builtin_popcountll(sides[Color::WHITE][PiecesEnum::KNIGHTS]) * 300;
    whiteScore += __builtin_popcountll(sides[Color::WHITE][PiecesEnum::BISHOPS]) * 300;
    whiteScore += __builtin_popcountll(sides[Color::WHITE][PiecesEnum::ROOKS]) * 500;
    whiteScore += __builtin_popcountll(sides[Color::WHITE][PiecesEnum::QUEEN]) * 900;

    blackScore += __builtin_popcountll(sides[Color::BLACK][PiecesEnum::PAWNS]) * 100;
    blackScore += __builtin_popcountll(sides[Color::BLACK][PiecesEnum::KNIGHTS]) * 300;
    blackScore += __builtin_popcountll(sides[Color::BLACK][PiecesEnum::BISHOPS]) * 300;
    blackScore += __builtin_popcountll(sides[Color::BLACK][PiecesEnum::ROOKS]) * 500;
    blackScore += __builtin_popcountll(sides[Color::BLACK][PiecesEnum::QUEEN]) * 900;

    auto evaluatePST = [](uint64_t bitboard, const int* pst, bool isBlack) {
        int score = 0;
        while (bitboard) {
            int sq = __builtin_ctzll(bitboard); // Trova l'indice del primo bit a 1 (0-63)            
            int pstSquare = isBlack ? (sq ^ 56) : sq; 
            score += pst[pstSquare];
            bitboard &= bitboard - 1;
        }
        return score;
    };
 
    whiteScore += evaluatePST(sides[Color::WHITE][PiecesEnum::KNIGHTS], knightPST, false);
    whiteScore += evaluatePST(sides[Color::WHITE][PiecesEnum::PAWNS], pawnPST, false);
    whiteScore += evaluatePST(sides[Color::WHITE][PiecesEnum::BISHOPS], bishopPST, false);
    whiteScore += evaluatePST(sides[Color::WHITE][PiecesEnum::ROOKS], rookPST, false);
    whiteScore += evaluatePST(sides[Color::WHITE][PiecesEnum::QUEEN], queenPST, false);
    whiteScore += evaluatePST(sides[Color::WHITE][PiecesEnum::KING], kingMiddlePST, false);

    blackScore += evaluatePST(sides[Color::BLACK][PiecesEnum::KNIGHTS], knightPST, true);
    blackScore += evaluatePST(sides[Color::BLACK][PiecesEnum::PAWNS], pawnPST, true);
    blackScore += evaluatePST(sides[Color::BLACK][PiecesEnum::BISHOPS], bishopPST, true);
    blackScore += evaluatePST(sides[Color::BLACK][PiecesEnum::ROOKS], rookPST, true);
    blackScore += evaluatePST(sides[Color::BLACK][PiecesEnum::QUEEN], queenPST, true);
    blackScore += evaluatePST(sides[Color::BLACK][PiecesEnum::KING], kingMiddlePST, true);

    if (__builtin_popcountll(sides[Color::WHITE][PiecesEnum::BISHOPS]) >= 2) whiteScore += 50;
    if (__builtin_popcountll(sides[Color::BLACK][PiecesEnum::BISHOPS]) >= 2) blackScore += 50;

    int evaluation = whiteScore - blackScore;

    return (sideToMove == Color::WHITE) ? evaluation : -evaluation;
}


uint64_t Board::GetHash() {
    uint64_t hash = 0;
    
    // Se tocca al Nero, cambiamo l'hash
    if (sideToMove == Color::BLACK) {
        hash ^= LookupTables::sideKey;
    }

    // Facciamo lo XOR di tutti i pezzi presenti sulla scacchiera
    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < 6; p++) {
            uint64_t bitboard = sides[c][p];
            while (bitboard != 0ULL) {
                int square = __builtin_ctzll(bitboard); // Trova l'indice del pezzo
                hash ^= LookupTables::pieceKeys[c][p][square];
                bitboard &= bitboard - 1; // Rimuove il pezzo contato
            }
        }
    }
    return hash;
}

bool Board::IsRepetition() {
    uint64_t currentHash = GetHash();
    
    int limit = std::max(0, historyPly - 6);
    for (int i = historyPly - 4; i >= limit; i -= 2) {
        if (positionHistory[i] == currentHash) {
            return true; // Trovata!
        }
    }
    return false;
}