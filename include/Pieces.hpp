#ifndef _PIECES
#define _PIECES

enum Color{
    WHITE,
    BLACK
};

namespace PiecesEnum {

    enum Type{
        PAWNS,
        ROOKS,
        KNIGHTS,
        BISHOPS,
        QUEEN,
        KING
    };

    static const Type All[] = { PAWNS, ROOKS, KNIGHTS, BISHOPS, QUEEN, KING };
}
#define NUM_PIECES 6

#endif