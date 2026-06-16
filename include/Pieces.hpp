#ifndef _PIECES
#define _PIECES

enum Color{
    WHITE,
    BLACK
};

enum PiecesEnum {
    PAWNS,
    ROOKS,
    KNIGHTS,
    BISHOPS,
    QUEEN,
    KING
};
#define NUM_PIECES 6

constexpr int GetBinValue(int e) {
    switch (e) {
        case PiecesEnum::PAWNS:   return 0b11111111;
        case PiecesEnum::ROOKS:   return 0b10000001;
        case PiecesEnum::KNIGHTS: return 0b01000010;
        case PiecesEnum::BISHOPS: return 0b00100100;
        case PiecesEnum::QUEEN:   return 0b00010000;
        case PiecesEnum::KING:    return 0b00001000;
        default: return 0b0000; // Fallback
    }
}
#endif