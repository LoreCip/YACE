#ifndef BOARD_HPP
#define BOARD_HPP

#include <cstdint>
#include <string>

#include "Types.hpp"
#include "Move.hpp"
#include "BitOperations.hpp"

struct UndoState {
    PieceType movedPiece;
    PieceType capturedPiece;
    uint8_t enPassantSquare;
    uint8_t castlingRights;  
    uint8_t halfMoveClock;   
};

class Board {
private:
    uint64_t sides[NUM_COLORS][NUM_PIECES] = {{(uint64_t)0}};
    uint64_t colorOccupation[NUM_COLORS] = {(uint64_t)0};
    uint64_t totalOccupation{(uint64_t)0};
    uint64_t freeCells{(uint64_t)0};
    
    uint8_t enPassantSquare{64};       
    uint8_t castlingRights{15};        
    uint8_t halfMoveClock{0};          
    Color sideToMove;

    uint64_t positionHistory[1024] = {(uint64_t) 0};
    UndoState history[1024]; 
    int historyPly = 0;

    inline void MovePiece(Color color, PieceType piece, int from, int to) {
        int c = ColorInt(color);
        int p = PieceInt(piece);
        sides[c][p] = clearBit(sides[c][p], from);
        sides[c][p] = setBit(sides[c][p], to);
    }
    inline void RemovePiece(int color, PieceType piece, int square) {
        int p = PieceInt(piece);
        sides[color][p] = clearBit(sides[color][p], square);
    }
    inline void RemovePiece(Color color, PieceType piece, int square) {
        RemovePiece(ColorInt(color), piece, square);
    }

public:
    void InitializeBoard();
    void InitializeFromFEN(const std::string& fen);
    void UpdateGlobalBoardState();

    bool MakeMove(Move move);  
    void UnmakeMove(Move move);
    
    bool IsSquareAttacked(int square, Color attackingColor) const;

    uint64_t GetHash() const;
    bool IsRepetition() const;
    std::string GetFEN() const;

    // --- GETTER ---
    inline uint64_t GetBitBoard(Color color, PieceType piece) const {
        return GetBitBoard(ColorInt(color), piece);
    }
    inline uint64_t GetBitBoard(int color, PieceType piece) const {
        return sides[color][PieceInt(piece)];
    }

    Color GetSideToMove() const {
        return sideToMove;
    }
    uint64_t GetColorOccupation(Color color) const {
        return colorOccupation[ColorInt(color)];
    }
    uint64_t GetTotalOccupation() const {
        return totalOccupation;
    }
    uint64_t GetFreeCells() const {
        return freeCells;
    }
    uint8_t GetEnPassantSquare() const {
        return enPassantSquare;
    }
    uint8_t GetCastlingRights() const {
        return castlingRights;
    }
    UndoState GetHistory(int ply) const {
        return history[ply];
    }
    int GetHistoryPly() const {
        return historyPly;
    }
    uint8_t GetHalfMoveClock() const {
        return halfMoveClock;
    }

    // --- SETTER ---
    void SetBitBoard(Color color, PieceType piece, uint64_t bitboard) {
        sides[ColorInt(color)][PieceInt(piece)] = bitboard;
    }
    inline void AddPiece(int color, PieceType piece, int square) {
        int p = PieceInt(piece);
        sides[color][p] = setBit(sides[color][p], square);
    }
    inline void AddPiece(Color color, PieceType piece, int square) {
        AddPiece(ColorInt(color), piece, square);
    }
};

#endif