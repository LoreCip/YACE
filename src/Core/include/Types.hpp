#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>
#include <initializer_list>

enum class Color : uint8_t {
    WHITE = 0,
    BLACK = 1
};
inline constexpr int ColorInt(Color c) { return static_cast<int>(c);}

inline Color operator!(Color c) {
    return c == Color::WHITE ? Color::BLACK : Color::WHITE;
}
inline bool operator==(int lhs, Color rhs) { return lhs == ColorInt(rhs); }
inline bool operator==(Color lhs, int rhs) { return ColorInt(lhs) == rhs; }
inline bool operator!=(int lhs, Color rhs) { return !(lhs == rhs); }
inline bool operator!=(Color lhs, int rhs) { return !(lhs == rhs); }

enum class PieceType : uint8_t {
    PAWN = 0,
    ROOK = 1,
    KNIGHT = 2,
    BISHOP = 3,
    QUEEN = 4,
    KING = 5,
    NONE = 6
};
namespace Pieces {
    constexpr auto All = {
        PieceType::PAWN,
        PieceType::ROOK,
        PieceType::KNIGHT,
        PieceType::BISHOP,
        PieceType::QUEEN,
        PieceType::KING
    };
}

inline constexpr int PieceInt(PieceType p) { return static_cast<int>(p);}


// Castling rights
namespace CastlingRights {
    constexpr uint8_t NONE = 0;
    constexpr uint8_t WK   = 1;  // 0001 (Re Bianco)
    constexpr uint8_t WQ   = 2;  // 0010 (Donna Bianca)
    constexpr uint8_t BK   = 4;  // 0100 (Re Nero)
    constexpr uint8_t BQ   = 8;  // 1000 (Donna Nera)
    constexpr uint8_t ALL  = 15; // 1111 (Tutti i diritti)
}

// Costants
constexpr int NUM_COLORS = 2;
constexpr int NUM_PIECES = 6;

#endif