#ifndef _PIECES
#define _PIECES

enum Color{
    WHITE,
    BLACK
};

static Color operator ! (const Color& self) {
    return self == Color::WHITE ? Color::BLACK : Color::WHITE;
}


namespace PiecesEnum {

    enum Type{
        PAWNS,
        ROOKS,
        KNIGHTS,
        BISHOPS,
        QUEEN,
        KING,
        NONE
    };    

    static const Type All[] = { PAWNS, ROOKS, KNIGHTS, BISHOPS, QUEEN, KING };
    const int pieceValues[6] = {100, 500, 300, 300, 900, 0};

}
#define NUM_PIECES 6

#endif