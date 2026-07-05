#include "Zobrist.hpp"

namespace Zobrist {

    uint64_t pieceKeys[2][6][64] = {{{0}}};
    uint64_t castlingKeys[16]    = {0};
    uint64_t enPassantKeys[65]   = {0};
    uint64_t sideKey             = 0;

    void Init() {
        uint64_t seed = 1070372ULL;
        auto rand64 = [&]() -> uint64_t {
            seed ^= seed >> 12;
            seed ^= seed << 25;
            seed ^= seed >> 27;
            return seed * 2685821657736338717ULL;
        };

        for (int i = 0; i < 16; i++) castlingKeys[i] = rand64();
        for (int i = 0; i < 65; i++) enPassantKeys[i] = rand64();

        for (int c = 0; c < 2; c++) {
            for (int p = 0; p < 6; p++) {
                for (int s = 0; s < 64; s++) {
                    pieceKeys[c][p][s] = rand64();
                }
            }
        }
        sideKey = rand64();
    }
}
