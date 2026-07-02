#include <cstdint>
#include <sys/types.h>
#include <string>
#include <sstream>

#include "Board.hpp"
#include "BitOperations.hpp"
#include "LookupTables.hpp"
#include "Move.hpp"
#include "Types.hpp"
#include "MoveGenerator.hpp"


/* PUBLIC  */

void Board::InitializeBoard(){
    
    sideToMove = Color::WHITE; 
    for (int color = 0; color < NUM_COLORS; color++){
        int startPos = color == Color::WHITE ? 8 : 48;
        for (int nPawn = 0; nPawn < 8; nPawn++){
            AddPiece(color, PieceType::PAWN, startPos);
            startPos++;
        }

        startPos = color == Color::WHITE ? 0 : 56;
        AddPiece(color, PieceType::ROOK, startPos);
        AddPiece(color, PieceType::ROOK, startPos + 7);
        AddPiece(color, PieceType::KNIGHT, startPos + 1);
        AddPiece(color, PieceType::KNIGHT, startPos + 6);
        AddPiece(color, PieceType::BISHOP, startPos + 2);
        AddPiece(color, PieceType::BISHOP, startPos + 5);
        AddPiece(color, PieceType::QUEEN, startPos + 3);
        AddPiece(color, PieceType::KING, startPos + 4);
    }
    
    UpdateGlobalBoardState();
}


bool Board::MakeMove(Move move) {
    int from = getMoveFrom(move);
    int to = getMoveTo(move);
    int flags = getMoveFlags(move);

    Color us = sideToMove;
    Color them = !us;

    PieceType pieceType = PieceType::NONE;
    for (const auto piece : Pieces::All) {
        if (getBit(GetBitBoard(us, piece), from)) {
            pieceType = piece;
            break;
        }
    }

    if (pieceType == PieceType::NONE) {
        return false;
    }

    bool captured = false;
    PieceType capturedPieceType = PieceType::NONE;
    int captureSquare = to; 

    if (isMoveEnPassant(flags)) {
        captured = true;
        capturedPieceType = PieceType::PAWN;
        captureSquare = (us == Color::WHITE) ? to - 8 : to + 8;
        RemovePiece(them, capturedPieceType, captureSquare);
    } else if (isMoveCapture(flags)) {
        captured = true;
        for (const auto piece : Pieces::All) {
            if (getBit(GetBitBoard(them, piece), to)) {
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

    if (pieceType == PieceType::PAWN || captured) {
        halfMoveClock = 0; // Reset
    } else {
        halfMoveClock++;
    }

    enPassantSquare = 64; // Reset di default
    if (pieceType == PieceType::PAWN && std::abs(from - to) == 16) {
        enPassantSquare = (from + to) / 2;
    }

    castlingRights &= LookupTables::castlingMasks[from];
    castlingRights &= LookupTables::castlingMasks[to];

    MovePiece(us, pieceType, from, to);

    if (pieceType == PieceType::KING && std::abs(from - to) == 2) {
        int rookFrom = 0, rookTo = 0;
        if (to == 6)  { rookFrom = 7;  rookTo = 5;  } // Bianco Corto
        else if (to == 2)  { rookFrom = 0;  rookTo = 3;  } // Bianco Lungo
        else if (to == 62) { rookFrom = 63; rookTo = 61; } // Nero Corto
        else if (to == 58) { rookFrom = 56; rookTo = 59; } // Nero Lungo
        
        MovePiece(us, PieceType::ROOK, rookFrom, rookTo);
    }

    if (isMovePromotion(move)) {
        PieceType promoPiece = getMovePromotionPiece(move);
        RemovePiece(us, PieceType::PAWN, to);
        AddPiece(us, promoPiece, to);
    }

    UpdateGlobalBoardState();
    sideToMove = them;

    uint64_t kingPosition = __builtin_ctzll(GetBitBoard(us, PieceType::KING));
    if (IsSquareAttacked(kingPosition, them)) {
        UnmakeMove(move);
        return false;
    }

    return true;
}

bool Board::IsSquareAttacked(int square, Color attackingColor) const {
    Color us = !attackingColor;
    
    // 1. Pawns
    uint64_t pawnTriggers = LookupTables::pawnAttacks[ColorInt(us)][square];
    if (pawnTriggers & GetBitBoard(attackingColor, PieceType::PAWN)) return true;

    // 2. Kinghts and King
    uint64_t knightTriggers = LookupTables::knightAttacks[square];
    if (knightTriggers & GetBitBoard(attackingColor, PieceType::KNIGHT)) return true;

    uint64_t kingTriggers = LookupTables::kingAttacks[square];
    if (kingTriggers & GetBitBoard(attackingColor, PieceType::KING)) return true;

    // 3. Bishops and Queen
    uint64_t bishopTriggers = (uint64_t)0;
    bishopTriggers |= MoveGen::ComputeRay(9, LookupTables::notColumnH, square, totalOccupation);
    bishopTriggers |= MoveGen::ComputeRay(7, LookupTables::notColumnA, square, totalOccupation);
    bishopTriggers |= MoveGen::ComputeRay(-9, LookupTables::notColumnA, square, totalOccupation);
    bishopTriggers |= MoveGen::ComputeRay(-7, LookupTables::notColumnH, square, totalOccupation);
    
    uint64_t diagonalThreaths = GetBitBoard(attackingColor, PieceType::BISHOP) | GetBitBoard(attackingColor, PieceType::QUEEN);
    if (bishopTriggers & diagonalThreaths) return true;

    // 4. Rooks and Queen
    uint64_t rookTriggers = (uint64_t)0;
    rookTriggers |= MoveGen::ComputeRay(8, ~0ULL, square, totalOccupation);
    rookTriggers |= MoveGen::ComputeRay(1, LookupTables::notColumnH, square, totalOccupation);
    rookTriggers |= MoveGen::ComputeRay(-8, ~0ULL, square, totalOccupation);
    rookTriggers |= MoveGen::ComputeRay(-1, LookupTables::notColumnA, square, totalOccupation);

    uint64_t orthogonalThreaths = GetBitBoard(attackingColor, PieceType::ROOK) | GetBitBoard(attackingColor, PieceType::QUEEN);
    if (rookTriggers & orthogonalThreaths) return true;

    return false;
}

void Board::UnmakeMove(Move move) {
    int from = getMoveFrom(move);
    int to = getMoveTo(move);
    int flags = getMoveFlags(move);

    Color us = !sideToMove;
    Color them = sideToMove;

    historyPly--;
    enPassantSquare = history[historyPly].enPassantSquare;
    castlingRights  = history[historyPly].castlingRights;
    halfMoveClock   = history[historyPly].halfMoveClock;
    
    PieceType pieceType = history[historyPly].movedPiece;
    PieceType capturedPiece = history[historyPly].capturedPiece;

    MovePiece(us, pieceType, to, from);

    PieceType promoPiece = getMovePromotionPiece(flags);
    if (promoPiece != PieceType::NONE) {
        RemovePiece(us, promoPiece, to);
        RemovePiece(us, PieceType::PAWN, to);
    }

    if (capturedPiece != PieceType::NONE) {
        int captureSquare = to;
        
        if (flags == FlagMap::ENPASS) {
            captureSquare = (us == Color::WHITE) ? to - 8 : to + 8;
        }
        AddPiece(them, capturedPiece, captureSquare);
    }

    if (pieceType == PieceType::KING && std::abs(from - to) == 2) {
        int rookFrom = 0, rookTo = 0;
        if (to == 6)  { rookFrom = 7;  rookTo = 5;  } // Bianco Corto
        else if (to == 2)  { rookFrom = 0;  rookTo = 3;  } // Bianco Lungo
        else if (to == 62) { rookFrom = 63; rookTo = 61; } // Nero Corto
        else if (to == 58) { rookFrom = 56; rookTo = 59; } // Nero Lungo
        
        MovePiece(us, PieceType::ROOK, rookTo, rookFrom);
    }

    sideToMove = us;
    UpdateGlobalBoardState();
}

void Board::UpdateGlobalBoardState() {
    for (int color = 0; color < NUM_COLORS; color++){
        colorOccupation[color] = 0;
        for (const auto piece : Pieces::All) {
            colorOccupation[color] |= GetBitBoard(color, piece);
        }
    }
    totalOccupation = colorOccupation[0] | colorOccupation[1];
    freeCells = ~totalOccupation;  
}

uint64_t Board::GetHash() const {
    uint64_t hash = 0;
    
    if (sideToMove == Color::BLACK) {
        hash ^= LookupTables::sideKey;
    }

    for (int c = 0; c < NUM_COLORS; c++) {
        for (int p = 0; p < 6; p++) {
            uint64_t bitboard = sides[c][p];
            while (bitboard != 0ULL) {
                int square = __builtin_ctzll(bitboard);
                hash ^= LookupTables::pieceKeys[c][p][square];
                bitboard &= bitboard - 1;
            }
        }
    }

    hash ^= LookupTables::castlingKeys[castlingRights];
    hash ^= LookupTables::enPassantKeys[enPassantSquare];
    return hash;
}

bool Board::IsRepetition() const {
    uint64_t currentHash = GetHash();
    if (historyPly < 4) return false; 

    for (int i = historyPly - 4; i >= historyPly - halfMoveClock; i -= 2){
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
            Color color = std::isupper(c) ? Color::WHITE : Color::BLACK;
            char lowerC = std::tolower(c);
            
            PieceType pieceType = PieceType::NONE;
            if (lowerC == 'p') pieceType = PieceType::PAWN;
            else if (lowerC == 'n') pieceType = PieceType::KNIGHT;
            else if (lowerC == 'b') pieceType = PieceType::BISHOP;
            else if (lowerC == 'r') pieceType = PieceType::ROOK;
            else if (lowerC == 'q') pieceType = PieceType::QUEEN;
            else if (lowerC == 'k') pieceType = PieceType::KING;

            if (pieceType != PieceType::NONE) {
                int square = rank * 8 + file;
                AddPiece(color, pieceType, square);
            }
            file++;
        }
    }

    sideToMove = (activeColor == "w") ? Color::WHITE : Color::BLACK;

    if (castling != "-") {
        for (char c : castling) {
            if (c == 'K') castlingRights |= CastlingRights::WK;
            if (c == 'Q') castlingRights |= CastlingRights::WQ;
            if (c == 'k') castlingRights |= CastlingRights::BK;
            if (c == 'q') castlingRights |= CastlingRights::BQ;
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

std::string Board::GetFEN() const {
    std::stringstream fen;

    for (int rank = 7; rank >= 0; rank--) {
        int emptySquares = 0;
        for (int file = 0; file < 8; file++) {
            int square = rank * 8 + file;
            
            int foundColor = -1;
            PieceType foundPiece = PieceType::NONE;

            for (int color = 0; color < 2; color++) {
                for (const auto piece : Pieces::All) {
                    if (getBit(GetBitBoard(color, piece), square)) {
                        foundColor = color;
                        foundPiece = piece;
                        break;
                    }
                }
                if (foundPiece != PieceType::NONE) break;
            }

            if (foundPiece != PieceType::NONE) {
                if (emptySquares > 0) {
                    fen << emptySquares;
                    emptySquares = 0;
                }
                
                char pieceChar = ' ';
                if (foundPiece == PieceType::PAWN)   pieceChar = 'p';
                else if (foundPiece == PieceType::KNIGHT) pieceChar = 'n';
                else if (foundPiece == PieceType::BISHOP) pieceChar = 'b';
                else if (foundPiece == PieceType::ROOK)   pieceChar = 'r';
                else if (foundPiece == PieceType::QUEEN)   pieceChar = 'q';
                else if (foundPiece == PieceType::KING)    pieceChar = 'k';

                if (foundColor == Color::WHITE) {
                    pieceChar = std::toupper(pieceChar);
                }
                fen << pieceChar;
            } else {
                emptySquares++;
            }
        }

        if (emptySquares > 0) {
            fen << emptySquares;
        }

        if (rank > 0) {
            fen << "/";
        }
    }

    fen << (sideToMove == Color::WHITE ? " w " : " b ");

    if (castlingRights == 0) {
        fen << "-";
    } else {
        if (castlingRights & CastlingRights::WK) fen << "K";
        if (castlingRights & CastlingRights::WQ) fen << "Q";
        if (castlingRights & CastlingRights::BK) fen << "k";
        if (castlingRights & CastlingRights::BQ) fen << "q";
    }

    fen << " ";
    if (enPassantSquare == 64) {
        fen << "-";
    } else {
        Color us = sideToMove;
        uint64_t myPawns = GetBitBoard(us, PieceType::PAWN);
        
        Color opponentColor = !us;
        uint64_t attackers = LookupTables::pawnAttacks[ColorInt(opponentColor)][enPassantSquare] & myPawns;

        if (attackers != 0ULL) {
            char epFile = 'a' + (enPassantSquare % 8);
            char epRank = '1' + (enPassantSquare / 8);
            fen << epFile << epRank;
        } else {
            fen << "-";
        }
    }

    // 5. Halfmove clock
    fen << " " << (int)halfMoveClock;
    fen << " " << (historyPly / 2 + 1);

    return fen.str();
}