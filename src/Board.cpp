#include <cstdint>
#include <sys/types.h>
#include <string>

#include "Board.hpp"
#include "BitOperations.hpp"
#include "LookupTables.hpp"
#include "Move.hpp"
#include "Pieces.hpp"

PiecesEnum::Type GetPromotionPiece(int flag) {
    switch (flag) {
        case FlagMap::PRQUEEN:  case FlagMap::PRCAPQUEEN:  return PiecesEnum::QUEEN;
        case FlagMap::PRROOK:   case FlagMap::PRCAPROOK:   return PiecesEnum::ROOKS;
        case FlagMap::PRBISHOP: case FlagMap::PRCAPBISHOP: return PiecesEnum::BISHOPS;
        case FlagMap::PRKNIGHT: case FlagMap::PRCAPKNIGHT: return PiecesEnum::KNIGHTS;
        default: return PiecesEnum::NONE;
    }
}

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
    
    int enemyColor = (color == Color::WHITE) ? Color::BLACK : Color::WHITE; 

    while (bitboard != (uint64_t) 0 ) {
        int square = __builtin_ctzll(bitboard);        
        uint64_t attacks = LookupTables::kingAttacks[square] & (~allies);
        allMoves |= attacks;
        bitboard = clearBit(bitboard, square);         
    }

    // --- Arrocco ---
    uint64_t castlingMoves = (uint64_t) 0;

    int kingSquare = (color == Color::WHITE) ? 4 : 60;
    bool isKingInCheck = IsSquareAttacked(kingSquare, enemyColor);

    if (!isKingInCheck) {
        
        auto canCastle = [&](uint64_t emptyMask, int transit1, int transit2, int destSq) {
            if (!(totalOccupation & emptyMask)) {
                if (!IsSquareAttacked(transit1, enemyColor) && 
                    !IsSquareAttacked(transit2, enemyColor)) {
                    castlingMoves |= (1ULL << destSq);
                }
            }
        };

        if (color == Color::WHITE) {
            if (castlingRights & WK_CASTLE) canCastle(LookupTables::maskWK, 5, 6, 6);
            if (castlingRights & WQ_CASTLE) canCastle(LookupTables::maskWQ, 3, 2, 2);
        } else {
            if (castlingRights & BK_CASTLE) canCastle(LookupTables::maskBK, 61, 62, 62);
            if (castlingRights & BQ_CASTLE) canCastle(LookupTables::maskBQ, 59, 58, 58);
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
        ray &= edge; 
        if (ray == (uint64_t) 0) break;
        ray = direction > 0 ? ray << direction : ray >> (-direction);
        if (ray == (uint64_t) 0) break;
        allMoves |= ray;
        if (ray & totalOccupation) break;
    }
    return allMoves;
}




/* PUBLIC  */

void Board::InitializeBoard(){
    
    sideToMove = Color::WHITE; 
    for (int color = 0; color < 2; color++){
        int startPos = color == Color::WHITE ? 8 : 48;
        for (int nPawn = 0; nPawn < 8; nPawn++){
            AddPiece(color, PiecesEnum::PAWNS, startPos);
        }

        startPos = color == Color::WHITE ? 0 : 56;
        AddPiece(color, PiecesEnum::ROOKS, startPos);
        AddPiece(color, PiecesEnum::ROOKS, startPos + 7);
        AddPiece(color, PiecesEnum::KNIGHTS, startPos + 1);
        AddPiece(color, PiecesEnum::KNIGHTS, startPos + 6);
        AddPiece(color, PiecesEnum::BISHOPS, startPos + 2);
        AddPiece(color, PiecesEnum::BISHOPS, startPos + 5);
        AddPiece(color, PiecesEnum::QUEEN, startPos + 3);
        AddPiece(color, PiecesEnum::KING, startPos + 4);
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
        RemovePiece(them, capturedPieceType, captureSquare);
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
        RemovePiece(them, capturedPieceType, to);
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

    castlingRights &= LookupTables::castlingMasks[from];
    castlingRights &= LookupTables::castlingMasks[to];

    MovePiece(us, pieceType, from, to);

    if (pieceType == PiecesEnum::KING && std::abs(from - to) == 2) {
        int rookFrom = 0, rookTo = 0;
        if (to == 6)  { rookFrom = 7;  rookTo = 5;  } // Bianco Corto
        else if (to == 2)  { rookFrom = 0;  rookTo = 3;  } // Bianco Lungo
        else if (to == 62) { rookFrom = 63; rookTo = 61; } // Nero Corto
        else if (to == 58) { rookFrom = 56; rookTo = 59; } // Nero Lungo
        
        MovePiece(us, PiecesEnum::ROOKS, rookFrom, rookTo);
    }

    PiecesEnum::Type promoPiece = GetPromotionPiece(flags);
    if (promoPiece != PiecesEnum::NONE) {
        sides[us][PiecesEnum::PAWNS] = clearBit(sides[us][PiecesEnum::PAWNS], to);
        sides[us][promoPiece] = setBit(sides[us][promoPiece], to);
    }

    UpdateGlobalBoardState();
    sideToMove = them;

    uint64_t kingPosition = __builtin_ctzll(sides[us][PiecesEnum::KING]);
    if (IsSquareAttacked(kingPosition, them)) {
        UnmakeMove(move);
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

    MovePiece(us, pieceType, to, from);

    PiecesEnum::Type promoPiece = GetPromotionPiece(flags);
    if (promoPiece != PiecesEnum::NONE) {
        sides[us][promoPiece] = clearBit(sides[us][promoPiece], to);
        sides[us][PiecesEnum::PAWNS] = clearBit(sides[us][PiecesEnum::PAWNS], to); 
    }

    if (capturedPiece != PiecesEnum::NONE) {
        int captureSquare = to;
        
        if (flags == FlagMap::ENPASS) {
            captureSquare = (us == Color::WHITE) ? to - 8 : to + 8;
        }
        AddPiece(them, capturedPiece, captureSquare);
    }

    if (pieceType == PiecesEnum::KING && std::abs(from - to) == 2) {
        int rookFrom = 0, rookTo = 0;
        if (to == 6)  { rookFrom = 7;  rookTo = 5;  } // Bianco Corto
        else if (to == 2)  { rookFrom = 0;  rookTo = 3;  } // Bianco Lungo
        else if (to == 62) { rookFrom = 63; rookTo = 61; } // Nero Corto
        else if (to == 58) { rookFrom = 56; rookTo = 59; } // Nero Lungo
        
        MovePiece(us, PiecesEnum::ROOKS, rookTo, rookFrom);
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
    int scores[2] = {0, 0};

    for (int color = 0; color < 2; color++) {
        for (const auto piece : PiecesEnum::All) {
            uint64_t bitboard = sides[color][piece];
            
            scores[color] += __builtin_popcountll(bitboard) * PiecesEnum::pieceValues[piece];
            
            while (bitboard) {
                int sq = __builtin_ctzll(bitboard);
                int pstSquare = (color == Color::BLACK) ? (sq ^ 56) : sq; 
                scores[color] += LookupTables::pstTables[piece][pstSquare];
                
                bitboard &= bitboard - 1;
            }
        }
        
        if (__builtin_popcountll(sides[color][PiecesEnum::BISHOPS]) >= 2) scores[color] += 50;
    }

    int evaluation = scores[Color::WHITE] - scores[Color::BLACK];
    return (sideToMove == Color::WHITE) ? evaluation : -evaluation;
}


uint64_t Board::GetHash() {
    uint64_t hash = 0;
    
    if (sideToMove == Color::BLACK) {
        hash ^= LookupTables::sideKey;
    }

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

    hash ^= LookupTables::castlingKeys[castlingRights];
    hash ^= LookupTables::enPassantKeys[enPassantSquare];
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
            file += (c - '0');
        } else {
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
                AddPiece(color, pieceType, square);
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