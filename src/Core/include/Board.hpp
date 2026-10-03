#ifndef BOARD_HPP
#define BOARD_HPP

#include <cstdint>
#include <string>

#include "Types.hpp"
#include "Move.hpp"
#include "BitOperations.hpp"
#include "LookupTables.hpp"

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
    
    uint8_t enPassantSquare{NO_SQUARE};
    uint8_t castlingRights{15};        
    uint8_t halfMoveClock{0};          
    Color sideToMove;

    uint64_t positionHistory[1024] = {(uint64_t) 0};
    UndoState history[1024]; 
    int historyPly = 0;
    uint64_t zobristHash = 0;

    // XOR delle chiavi Zobrist di arrocco ed en passant correnti (le aggiunge o le toglie).
    void HashCastlingAndEnPassant() {
        zobristHash ^= Zobrist::castlingKeys[castlingRights];
        if (enPassantSquare != NO_SQUARE) zobristHash ^= Zobrist::enPassantKeys[enPassantSquare];
    }
    void UpdateOccupation() {
        totalOccupation = colorOccupation[0] | colorOccupation[1];
    }
    // hashBefore = hash completo della posizione prima della mossa (usato da IsRepetition).
    void PushHistory(PieceType moved, PieceType captured, uint64_t hashBefore) {
        positionHistory[historyPly] = hashBefore;
        history[historyPly++] = {moved, captured, enPassantSquare, castlingRights, halfMoveClock};
    }

    inline void MovePiece(Color color, PieceType piece, int from, int to) {
        RemovePiece(color, piece, from);
        AddPiece(color, piece, to);
    }
    inline void RemovePiece(int color, PieceType piece, int square) {
        int p = PieceInt(piece);
        sides[color][p] = clearBit(sides[color][p], square);
        colorOccupation[color] = clearBit(colorOccupation[color], square);
        zobristHash ^= Zobrist::pieceKeys[color][p][square];
    }
    inline void RemovePiece(Color color, PieceType piece, int square) {
        RemovePiece(ColorInt(color), piece, square);
    }
public:
    void InitializeBoard();
    void InitializeFromFEN(const std::string& fen);

    bool MakeMove(Move move);  
    void UnmakeMove(Move move);

    void MakeNullMove();
    void UnmakeNullMove();
    
    bool IsSquareAttacked(int square, Color attackingColor) const;

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
        return ~totalOccupation;
    }
    uint8_t GetEnPassantSquare() const {
        return enPassantSquare;
    }
    uint8_t GetCastlingRights() const {
        return castlingRights;
    }
    const UndoState& GetHistory(int ply) const {
        return history[ply];
    }
    int GetHistoryPly() const {
        return historyPly;
    }
    uint8_t GetHalfMoveClock() const {
        return halfMoveClock;
    }
    inline PieceType GetPieceOnSquare(int sq) const {
        uint64_t mask = (1ULL << sq);
        
        for (int p = 0; p < NUM_PIECES; ++p) {
            if (sides[0][p] & mask) return static_cast<PieceType>(p); // Colore WHITE
            if (sides[1][p] & mask) return static_cast<PieceType>(p); // Colore BLACK
        }
        return PieceType::NONE;
    }

    uint64_t GetHash() const {
        return zobristHash;
    }

    // --- SETTER ---
    inline void AddPiece(int color, PieceType piece, int square) {        
        int p = PieceInt(piece);
        sides[color][p] = setBit(sides[color][p], square);
        colorOccupation[color] = setBit(colorOccupation[color], square);
        zobristHash ^= Zobrist::pieceKeys[color][p][square];
    }
    inline void AddPiece(Color color, PieceType piece, int square) {
        AddPiece(ColorInt(color), piece, square);
    }
    
};

#endif