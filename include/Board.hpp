#ifndef _BOARD
#define _BOARD

#include <cstdint>
#include "Pieces.hpp"
#include "Move.hpp"
#include "BitOperations.hpp"

struct UndoState {
    PiecesEnum::Type movedPiece;
    PiecesEnum::Type capturedPiece;
    // int enPassantSquare; uint8_t castlingRights;
};

class Board {
private:
    uint64_t sides[2][NUM_PIECES] = {(uint64_t)0};
    uint64_t colorOccupation[2] = {(uint64_t)0};
    uint64_t totalOccupation{(uint64_t)0};
    uint64_t freeCells{(uint64_t)0};
    uint64_t positionHistory[1024] = {(uint64_t) 0};
    UndoState history[1024] = {{PiecesEnum::NONE, PiecesEnum::NONE}};;
    int historyPly = 0;
    int sideToMove;

    uint64_t ComputePawnMoves(int color, uint64_t bitboard);
    uint64_t ComputeKnightMoves(int color, uint64_t bitboard);
    uint64_t ComputeKingMoves(int color, uint64_t bitboard);
    uint64_t ComputeBishopMoves(int color, uint64_t bitboard);
    uint64_t ComputeRookMoves(int color, uint64_t bitboard);
    uint64_t ComputeQueenMoves(int color, uint64_t bitboard);

    uint64_t ComputeRay(int direction, uint64_t edge, int square);


public:
    void InitializeBoard();
    void UpdateGlobalBoardState();

    bool MakeMove(Move move);
    void UnmakeMove(Move move);
    int Evaluate();
    bool IsSquareAttacked(int square, int attackingColor);

    uint64_t GetHash();
    bool IsRepetition();

    uint64_t GetGeneratedMoves(int color, uint64_t bitboard, PiecesEnum::Type piece);

    uint64_t GetPieceBitBoard(int color, PiecesEnum::Type piece){
        return sides[color][piece];
    }
    void SetPieceBitBoard(int color, PiecesEnum::Type pieceType, uint64_t bitboard) {
        sides[color][pieceType] = bitboard;
    }
    void AddPiece(int color, PiecesEnum::Type pieceType, int square) {
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

};

#endif