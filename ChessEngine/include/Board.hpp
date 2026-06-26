#ifndef _BOARD
#define _BOARD

#include <sstream>
#include <cstdint>
#include "Pieces.hpp"
#include "Move.hpp"
#include "BitOperations.hpp"

enum CastlingRights {
    WK_CASTLE = 1, // 0001 (Re Bianco)
    WQ_CASTLE = 2, // 0010 (Donna Bianca)
    BK_CASTLE = 4, // 0100 (Re Nero)
    BQ_CASTLE = 8  // 1000 (Donna Nera)
};

struct UndoState {
    PiecesEnum::Type movedPiece;
    PiecesEnum::Type capturedPiece;
    uint8_t enPassantSquare; // 0-63 per la casella, 64 o 255 per "nessun en passant"
    uint8_t castlingRights;  // Maschera di bit per i 4 arrocchi
    uint8_t halfMoveClock;   // Contatore per la regola delle 50 mosse
};

class Board {
private:
    uint64_t sides[2][NUM_PIECES] = {{(uint64_t)0}};
    uint64_t colorOccupation[2] = {(uint64_t)0};
    uint64_t totalOccupation{(uint64_t)0};
    uint64_t freeCells{(uint64_t)0};
    
    // Stato del gioco e History
    uint8_t enPassantSquare{64};       // Inizializzato a 64 (nessuna casella)
    uint8_t castlingRights{15};        // Inizializzato a 15 (1111 in binario: tutti permessi)
    uint8_t halfMoveClock{0};          // Si resetta a 0 dopo catture o mosse di pedone
    int sideToMove;

    uint64_t positionHistory[1024] = {(uint64_t) 0};
    UndoState history[1024]; 
    int historyPly = 0;

    bool useNnue = true;

    uint64_t ComputePawnMoves(int color, uint64_t bitboard);
    uint64_t ComputeKnightMoves(int color, uint64_t bitboard);
    uint64_t ComputeKingMoves(int color, uint64_t bitboard);
    uint64_t ComputeBishopMoves(int color, uint64_t bitboard);
    uint64_t ComputeRookMoves(int color, uint64_t bitboard);
    uint64_t ComputeQueenMoves(int color, uint64_t bitboard);

    uint64_t ComputeRay(int direction, uint64_t edge, int square);

    inline void MovePiece(int color, PiecesEnum::Type piece, int from, int to) {
        sides[color][piece] = clearBit(sides[color][piece], from);
        sides[color][piece] = setBit(sides[color][piece], to);
    }
    inline void RemovePiece(int color, PiecesEnum::Type piece, int square) {
        sides[color][piece] = clearBit(sides[color][piece], square);
    }


public:
    void InitializeBoard();
    void InitializeFromFEN(const std::string& fen);
    void UpdateGlobalBoardState();

    bool MakeMove(Move move);
    void UnmakeMove(Move move);
    bool IsSquareAttacked(int square, int attackingColor);

    uint64_t GetHash();
    bool IsRepetition();

    uint64_t GetGeneratedMoves(int color, uint64_t bitboard, PiecesEnum::Type piece);
    PiecesEnum::Type GetPromotionPiece(int flag);

    uint64_t GetBitBoard(int color, PiecesEnum::Type piece){
        return sides[color][piece];
    }
    void SetBitBoard(int color, PiecesEnum::Type pieceType, uint64_t bitboard) {
        sides[color][pieceType] = bitboard;
    }
    inline void AddPiece(int color, PiecesEnum::Type pieceType, int square) {
        sides[color][pieceType] = setBit(sides[color][pieceType], square);
    }
    int GetSideToMove() {
        return sideToMove;
    }
    void SetSideToMove(Color color) {
        sideToMove = color;
    }
    uint64_t GetColorOccupation(int color){
        return colorOccupation[color];
    }
    uint64_t GetTotalOccupation(){
        return totalOccupation;
    }
    uint64_t GetFreeCells(){
        return freeCells;
    }
    uint8_t GetEnPassantSquare() {
        return enPassantSquare;
    }
    uint8_t GetCastlingRights() {
        return castlingRights;
    }
    uint8_t GetHalfMoveClock() {
        return halfMoveClock;
    }
    UndoState GetHistory(int ply){
        return history[ply];
    }
    int GetHistoryPly(){
        return historyPly;
    }

};

#endif