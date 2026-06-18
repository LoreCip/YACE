#ifndef _BOARD
#define _BOARD

#include <cstdint>
#include "Pieces.hpp"
#include "Move.hpp"

struct UndoState {
    PiecesEnum::Type capturedPiece;
    // int enPassantSquare; uint8_t castlingRights;
};

class Board {
private:
    uint64_t sides[2][NUM_PIECES] = {(uint64_t)0};
    uint64_t colorOccupation[2] = {(uint64_t)0};
    uint64_t totalOccupation{(uint64_t)0};
    uint64_t freeCells{(uint64_t)0};
    UndoState history[1024] = {PiecesEnum::NONE};
    int historyPly = 0;
    int sideToMove;

    uint64_t ComputePawnMoves(int color, uint64_t bitboard);
    uint64_t ComputeKnightMoves(int color, uint64_t bitboard);
    uint64_t ComputeKingMoves(int color, uint64_t bitboard);
    uint64_t ComputeBishopMoves(int color, uint64_t bitboard);
    uint64_t ComputeRookMoves(int color, uint64_t bitboard);
    uint64_t ComputeQueenMoves(int color, uint64_t bitboard);

    uint64_t ComputeRay(int direction, uint64_t edge, int square);
    void UpdateGlobalBoardState();

public:
    void InitializeBoard();
    uint64_t GetColorOccupation(int color);
    uint64_t GetTotalOccupation();
    uint64_t GetFreeCells();
    uint64_t GetGeneratedMoves(int color, uint64_t bitboard, PiecesEnum::Type piece);
    bool IsSquareAttacked(int square, int attackingColor);

    bool MakeMove(Move move);
    void UnmakeMove(Move move);
};

#endif