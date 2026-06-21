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
    uint64_t westCapture = color == Color::WHITE ? ((bitboard & LookupTables::notColumnA) << 7) & enemyCells :
                                            ((bitboard & LookupTables::notColumnH) >> 9) & enemyCells;
    uint64_t estCapture  = color == Color::WHITE ? ((bitboard & LookupTables::notColumnH) << 9) & enemyCells :
                                            ((bitboard & LookupTables::notColumnA) >> 7) & enemyCells;

    // --- 4. En passant ---
    uint64_t epMoves = (uint64_t) 0;
    if (enPassantSquare != 64 /* == -1 */) {
        uint64_t epBB = (uint64_t)1 << enPassantSquare;
        
        uint64_t epWestCapture = color == Color::WHITE ? 
            ((bitboard & LookupTables::notColumnA) << 7) & epBB :
            ((bitboard & LookupTables::notColumnH) >> 9) & epBB;
            
        uint64_t epEstCapture = color == Color::WHITE ? 
            ((bitboard & LookupTables::notColumnH) << 9) & epBB :
            ((bitboard & LookupTables::notColumnA) >> 7) & epBB;
            
        epMoves = epWestCapture | epEstCapture;
    }

    return allMoves | estCapture | westCapture | epMoves;
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
    
    // Aggiunta la dichiarazione mancante di enemyColor
    int enemyColor = (color == Color::WHITE) ? Color::BLACK : Color::WHITE; 

    while (bitboard != (uint64_t) 0 ) {
        int square = __builtin_ctzll(bitboard);        
        uint64_t attacks = LookupTables::kingAttacks[square] & (~allies);
        allMoves |= attacks;
        bitboard = clearBit(bitboard, square);         
    }

    // --- Arrocco ---
    uint64_t castlingMoves = (uint64_t) 0;

    if (color == Color::WHITE) {
        // Arrocco Corto Bianco (e1 -> g1)
        if (castlingRights & WK_CASTLE) {
            if (!(totalOccupation & 0x60ULL)) { // f1 e g1 sono libere? (0x60 = bit 5 e 6)
                if (!IsSquareAttacked(4, enemyColor) && !IsSquareAttacked(5, enemyColor) && !IsSquareAttacked(6, enemyColor)) {
                    castlingMoves |= (1ULL << 6); // Aggiungi g1 alle mosse
                }
            }
        }
        // Arrocco Lungo Bianco (e1 -> c1)
        if (castlingRights & WQ_CASTLE) { 
            if (!(totalOccupation & 0xEULL)) { // b1, c1, d1 sono libere? (0xE = bit 1, 2, 3)
                if (!IsSquareAttacked(4, enemyColor) && !IsSquareAttacked(3, enemyColor) && !IsSquareAttacked(2, enemyColor)) {
                    castlingMoves |= (1ULL << 2); // Aggiungi c1 alle mosse
                }
            }
        }
    } else {
        // Arrocco Corto Nero (e8 -> g8)
        if (castlingRights & BK_CASTLE) { 
            if (!(totalOccupation & (0x60ULL << 56))) { // f8 e g8 libere?
                if (!IsSquareAttacked(60, enemyColor) && !IsSquareAttacked(61, enemyColor) && !IsSquareAttacked(62, enemyColor)) {
                    castlingMoves |= (1ULL << 62);
                }
            }
        }
        // Arrocco Lungo Nero (e8 -> c8)
        if (castlingRights & BQ_CASTLE) { 
            if (!(totalOccupation & (0xEULL << 56))) { // b8, c8, d8 libere?
                if (!IsSquareAttacked(60, enemyColor) && !IsSquareAttacked(59, enemyColor) && !IsSquareAttacked(58, enemyColor)) {
                    castlingMoves |= (1ULL << 58);
                }
            }
        }
    }

    return allMoves | castlingMoves;
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

bool Board::MakeMove(Move move) {
    int from = getMoveFrom(move);
    int to = getMoveTo(move);
    int flags = getMoveFlags(move);

    int us = sideToMove;
    int them = (us == Color::WHITE) ? Color::BLACK : Color::WHITE;

    PiecesEnum::Type pieceType = PiecesEnum::NONE;
    for (const auto piece : PiecesEnum::All) {
        if (getBit(sides[us][piece], from)) {
            pieceType = piece;
            break;
        }
    }

    bool captured = false;
    PiecesEnum::Type capturedPieceType = PiecesEnum::NONE;
    int captureSquare = to; 

    if (flags == FlagMap::ENPASS) {
        captured = true;
        capturedPieceType = PiecesEnum::PAWNS;
        captureSquare = (us == Color::WHITE) ? to - 8 : to + 8;
        sides[them][capturedPieceType] = clearBit(sides[them][capturedPieceType], captureSquare);
    } 
    else if (flags == FlagMap::CAPTURE || flags == FlagMap::PRCAPBISHOP ||
             flags == FlagMap::PRCAPKNIGHT || flags == FlagMap::PRCAPQUEEN || flags == FlagMap::PRCAPROOK) {
        captured = true;
        for (const auto piece : PiecesEnum::All) {
            if (getBit(sides[them][piece], to)) {
                capturedPieceType = piece;
                break;
            }
        }
        sides[them][capturedPieceType] = clearBit(sides[them][capturedPieceType], to);
    }

    positionHistory[historyPly] = GetHash();
    history[historyPly].movedPiece = pieceType;
    history[historyPly].capturedPiece = capturedPieceType;
    history[historyPly].enPassantSquare = enPassantSquare;
    history[historyPly].castlingRights = castlingRights;
    history[historyPly].halfMoveClock = halfMoveClock;
    historyPly++;

    if (pieceType == PiecesEnum::PAWNS || captured) {
        halfMoveClock = 0; // Reset
    } else {
        halfMoveClock++;
    }

    enPassantSquare = 64; // Reset di default
    if (pieceType == PiecesEnum::PAWNS && std::abs(from - to) == 16) {
        enPassantSquare = (from + to) / 2;
    }

    if (pieceType == PiecesEnum::KING) {
        castlingRights &= (us == Color::WHITE) ? ~(WK_CASTLE | WQ_CASTLE) : ~(BK_CASTLE | BQ_CASTLE);
    }
    if (from == 7 || to == 7) castlingRights &= ~WK_CASTLE;   // Torre H1
    if (from == 0 || to == 0) castlingRights &= ~WQ_CASTLE;   // Torre A1
    if (from == 63 || to == 63) castlingRights &= ~BK_CASTLE; // Torre H8
    if (from == 56 || to == 56) castlingRights &= ~BQ_CASTLE; // Torre A8

    sides[us][pieceType] = clearBit(sides[us][pieceType], from);
    sides[us][pieceType] = setBit(sides[us][pieceType], to);

    if (pieceType == PiecesEnum::KING && std::abs(from - to) == 2) {
        int rookFrom = 0, rookTo = 0;
        if (to == 6)  { rookFrom = 7;  rookTo = 5;  } // Bianco Corto
        else if (to == 2)  { rookFrom = 0;  rookTo = 3;  } // Bianco Lungo
        else if (to == 62) { rookFrom = 63; rookTo = 61; } // Nero Corto
        else if (to == 58) { rookFrom = 56; rookTo = 59; } // Nero Lungo
        
        sides[us][PiecesEnum::ROOKS] = clearBit(sides[us][PiecesEnum::ROOKS], rookFrom);
        sides[us][PiecesEnum::ROOKS] = setBit(sides[us][PiecesEnum::ROOKS], rookTo);
    }

    if (flags == FlagMap::PRQUEEN || flags == FlagMap::PRCAPQUEEN) {
        sides[us][PiecesEnum::PAWNS] = clearBit(sides[us][PiecesEnum::PAWNS], to);
        sides[us][PiecesEnum::QUEEN] = setBit(sides[us][PiecesEnum::QUEEN], to);
    }
    else if (flags == FlagMap::PRROOK || flags == FlagMap::PRCAPROOK) {
        sides[us][PiecesEnum::PAWNS] = clearBit(sides[us][PiecesEnum::PAWNS], to);
        sides[us][PiecesEnum::ROOKS] = setBit(sides[us][PiecesEnum::ROOKS], to);
    }
    else if (flags == FlagMap::PRBISHOP || flags == FlagMap::PRCAPBISHOP) {
        sides[us][PiecesEnum::PAWNS] = clearBit(sides[us][PiecesEnum::PAWNS], to);
        sides[us][PiecesEnum::BISHOPS] = setBit(sides[us][PiecesEnum::BISHOPS], to);
    }
    else if (flags == FlagMap::PRKNIGHT || flags == FlagMap::PRCAPKNIGHT) {
        sides[us][PiecesEnum::PAWNS] = clearBit(sides[us][PiecesEnum::PAWNS], to);
        sides[us][PiecesEnum::KNIGHTS] = setBit(sides[us][PiecesEnum::KNIGHTS], to);
    }

    UpdateGlobalBoardState();
    sideToMove = them;

    uint64_t kingPosition = __builtin_ctzll(sides[us][PiecesEnum::KING]);
    if (IsSquareAttacked(kingPosition, them)) {
        UnmakeMove(move); // La mossa è illegale, annulliamo tutto
        return false;
    }

    return true;
}

bool Board::IsSquareAttacked(int square, int attackingColor) {
    int us = (attackingColor == Color::WHITE) ? Color::BLACK : Color::WHITE;

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
    int from = getMoveFrom(move);
    int to = getMoveTo(move);
    int flags = getMoveFlags(move);

    int us = (sideToMove == Color::WHITE) ? Color::BLACK : Color::WHITE;
    int them = (us == Color::WHITE) ? Color::BLACK : Color::WHITE;

    historyPly--;
    enPassantSquare = history[historyPly].enPassantSquare;
    castlingRights  = history[historyPly].castlingRights;
    halfMoveClock   = history[historyPly].halfMoveClock;
    
    PiecesEnum::Type pieceType = history[historyPly].movedPiece;
    PiecesEnum::Type capturedPiece = history[historyPly].capturedPiece;

    sides[us][pieceType] = setBit(sides[us][pieceType], from);
    sides[us][pieceType] = clearBit(sides[us][pieceType], to);

    if (flags == FlagMap::PRQUEEN || flags == FlagMap::PRCAPQUEEN) {
        sides[us][PiecesEnum::QUEEN] = clearBit(sides[us][PiecesEnum::QUEEN], to);
        sides[us][PiecesEnum::PAWNS] = clearBit(sides[us][PiecesEnum::PAWNS], to); // Forza la pulizia in 'to'
    }
    else if (flags == FlagMap::PRROOK || flags == FlagMap::PRCAPROOK) {
        sides[us][PiecesEnum::ROOKS] = clearBit(sides[us][PiecesEnum::ROOKS], to);
        sides[us][PiecesEnum::PAWNS] = clearBit(sides[us][PiecesEnum::PAWNS], to);
    }
    else if (flags == FlagMap::PRBISHOP || flags == FlagMap::PRCAPBISHOP) {
        sides[us][PiecesEnum::BISHOPS] = clearBit(sides[us][PiecesEnum::BISHOPS], to);
        sides[us][PiecesEnum::PAWNS] = clearBit(sides[us][PiecesEnum::PAWNS], to);
    }
    else if (flags == FlagMap::PRKNIGHT || flags == FlagMap::PRCAPKNIGHT) {
        sides[us][PiecesEnum::KNIGHTS] = clearBit(sides[us][PiecesEnum::KNIGHTS], to);
        sides[us][PiecesEnum::PAWNS] = clearBit(sides[us][PiecesEnum::PAWNS], to);
    }

    if (capturedPiece != PiecesEnum::NONE) {
        int captureSquare = to;
        
        if (flags == FlagMap::ENPASS) {
            captureSquare = (us == Color::WHITE) ? to - 8 : to + 8;
        }
        
        sides[them][capturedPiece] = setBit(sides[them][capturedPiece], captureSquare);
    }

    if (pieceType == PiecesEnum::KING && std::abs(from - to) == 2) {
        int rookFrom = 0, rookTo = 0;
        if (to == 6)  { rookFrom = 7;  rookTo = 5;  } // Bianco Corto
        else if (to == 2)  { rookFrom = 0;  rookTo = 3;  } // Bianco Lungo
        else if (to == 62) { rookFrom = 63; rookTo = 61; } // Nero Corto
        else if (to == 58) { rookFrom = 56; rookTo = 59; } // Nero Lungo
        
        sides[us][PiecesEnum::ROOKS] = clearBit(sides[us][PiecesEnum::ROOKS], rookTo);
        sides[us][PiecesEnum::ROOKS] = setBit(sides[us][PiecesEnum::ROOKS], rookFrom);
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
    
    /**/
    whiteScore += evaluatePST(sides[Color::WHITE][PiecesEnum::KNIGHTS], LookupTables::knightPST, false);
    whiteScore += evaluatePST(sides[Color::WHITE][PiecesEnum::PAWNS], LookupTables::pawnPST, false);
    whiteScore += evaluatePST(sides[Color::WHITE][PiecesEnum::BISHOPS], LookupTables::bishopPST, false);
    whiteScore += evaluatePST(sides[Color::WHITE][PiecesEnum::ROOKS], LookupTables::rookPST, false);
    whiteScore += evaluatePST(sides[Color::WHITE][PiecesEnum::QUEEN], LookupTables::queenPST, false);
    whiteScore += evaluatePST(sides[Color::WHITE][PiecesEnum::KING], LookupTables::kingMiddlePST, false);

    blackScore += evaluatePST(sides[Color::BLACK][PiecesEnum::KNIGHTS], LookupTables::knightPST, true);
    blackScore += evaluatePST(sides[Color::BLACK][PiecesEnum::PAWNS], LookupTables::pawnPST, true);
    blackScore += evaluatePST(sides[Color::BLACK][PiecesEnum::BISHOPS], LookupTables::bishopPST, true);
    blackScore += evaluatePST(sides[Color::BLACK][PiecesEnum::ROOKS], LookupTables::rookPST, true);
    blackScore += evaluatePST(sides[Color::BLACK][PiecesEnum::QUEEN], LookupTables::queenPST, true);
    blackScore += evaluatePST(sides[Color::BLACK][PiecesEnum::KING], LookupTables::kingMiddlePST, true);

    if (__builtin_popcountll(sides[Color::WHITE][PiecesEnum::BISHOPS]) >= 2) whiteScore += 50;
    if (__builtin_popcountll(sides[Color::BLACK][PiecesEnum::BISHOPS]) >= 2) blackScore += 50;

    int evaluation = whiteScore - blackScore;

    return (sideToMove == Color::WHITE) ? evaluation :  - evaluation;
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
    if (historyPly < 4) return false; 

    for (int i = historyPly - 4; i >= 0; i -= 2) {
        if (positionHistory[i] == currentHash) {
            return true; 
        }
    }
    return false;
}


void Board::InitializeFromFEN(const std::string& fen) {
    // 1. Reset completo di tutte le bitboard e variabili di stato
    for (int c = 0; c < 2; c++) {
        for (int p = 0; p < NUM_PIECES; p++) {
            sides[c][p] = 0ULL;
        }
    }
    historyPly = 0;
    halfMoveClock = 0;
    enPassantSquare = 64;
    castlingRights = 0;

    std::stringstream ss(fen);
    std::string pieces, activeColor, castling, enPassant, halfMove, fullMove;
    
    ss >> pieces >> activeColor >> castling >> enPassant >> halfMove >> fullMove;

    int rank = 7;
    int file = 0;

    for (char c : pieces) {
        if (c == '/') {
            rank--;
            file = 0;
        } else if (std::isdigit(c)) {
            file += (c - '0'); // Salta le caselle vuote
        } else {
            // Determiniamo il colore (Maiuscolo = Bianco, Minuscolo = Nero)
            int color = std::isupper(c) ? Color::WHITE : Color::BLACK;
            char lowerC = std::tolower(c);
            
            PiecesEnum::Type pieceType = PiecesEnum::NONE;
            if (lowerC == 'p') pieceType = PiecesEnum::PAWNS;
            else if (lowerC == 'n') pieceType = PiecesEnum::KNIGHTS;
            else if (lowerC == 'b') pieceType = PiecesEnum::BISHOPS;
            else if (lowerC == 'r') pieceType = PiecesEnum::ROOKS;
            else if (lowerC == 'q') pieceType = PiecesEnum::QUEEN;
            else if (lowerC == 'k') pieceType = PiecesEnum::KING;

            if (pieceType != PiecesEnum::NONE) {
                int square = rank * 8 + file;
                sides[color][pieceType] = setBit(sides[color][pieceType], square);
            }
            file++;
        }
    }

    sideToMove = (activeColor == "w") ? Color::WHITE : Color::BLACK;

    if (castling != "-") {
        for (char c : castling) {
            if (c == 'K') castlingRights |= WK_CASTLE;
            if (c == 'Q') castlingRights |= WQ_CASTLE;
            if (c == 'k') castlingRights |= BK_CASTLE;
            if (c == 'q') castlingRights |= BQ_CASTLE;
        }
    }

    if (enPassant != "-") {
        int epFile = enPassant[0] - 'a';
        int epRank = enPassant[1] - '1';
        enPassantSquare = epRank * 8 + epFile;
    } else {
        enPassantSquare = 64; // Nessuna casella EP
    }

    if (!halfMove.empty()) {
        halfMoveClock = std::stoi(halfMove);
    }

    UpdateGlobalBoardState();
}