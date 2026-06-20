#include "LookupTables.hpp"
#include "BitOperations.hpp"
#include "Pieces.hpp"

namespace LookupTables {

    uint64_t knightAttacks[64] = {(uint64_t)0};
    uint64_t kingAttacks[64]   = {(uint64_t)0};
    uint64_t pawnAttacks[2][64] = {(uint64_t)0};

    uint64_t pieceKeys[2][6][64];
    uint64_t sideKey;

    void init(){
        // --- ZOBRIST ---
        uint64_t pseudoRandom = 1070372ULL;
        auto rand64 = [&]() -> uint64_t {
            pseudoRandom ^= pseudoRandom >> 12;
            pseudoRandom ^= pseudoRandom << 25;
            pseudoRandom ^= pseudoRandom >> 27;
            return pseudoRandom * 2685821657736338717ULL;
        };

        for(int c = 0; c < 2; c++) {
            for(int p = 0; p < 6; p++) {
                for(int s = 0; s < 64; s++) {
                    pieceKeys[c][p][s] = rand64();
                }
            }
        }
        sideKey = rand64();
        // --------------------------------

        for (int i = 0; i < 64; i++){    
            /****************************************/
            /*       Compute the knight moves       */
            /****************************************/
            // A. Create empty board
            uint64_t bitboard = (uint64_t)0;
            bitboard = setBit(bitboard, i); 

            // B. Number of squares that can be attacked
            uint64_t attacks = (uint64_t)0;

            // (Traslation) AND (Mask)
            attacks |= ((bitboard & notColumnH) << 17);        // Nord-Nord-Est
            attacks |= ((bitboard & notColumnHDouble) << 10);  // Nord-Est-Est
            attacks |= ((bitboard & notColumnHDouble) >> 6);   // Sud-Est-Est
            attacks |= ((bitboard & notColumnH) >> 15);        // Sud-Sud-Est

            attacks |= ((bitboard & notColumnA) >> 17);        // Sud-Sud-Ovest
            attacks |= ((bitboard & notColumnADouble) >> 10);  // Sud-Ovest-Ovest
            attacks |= ((bitboard & notColumnADouble) << 6);   // Nord-Ovest-Ovest
            attacks |= ((bitboard & notColumnA) << 15);        // Nord-Nord-Ovest

            knightAttacks[i] = attacks;
            

            /****************************************/
            /*       Compute the king moves         */
            /****************************************/
            bitboard = (uint64_t)0;
            bitboard = setBit(bitboard, i); 

            attacks = (uint64_t)0;

            attacks |= ((bitboard & notColumnA) >> 1);  // Ovest (-1)
            attacks |= ((bitboard & notColumnH) << 1);  // Est (+1)
            attacks |= (bitboard << 8);                 // Nord (+8, No mask)
            attacks |= (bitboard >> 8);                 // Sud (-8,  No mask)

            attacks |= ((bitboard & notColumnH) << 9);  // Nord-Est (+8 +1)
            attacks |= ((bitboard & notColumnA) << 7);  // Nord-Ovest (+8 -1)
            attacks |= ((bitboard & notColumnH) >> 7);  // Sud-Est (-8 +1)
            attacks |= ((bitboard & notColumnA) >> 9);  // Sud-Ovest (-8 -1)

            kingAttacks[i] = attacks;

            /****************************************/
            /*       Compute the pawn captures      */
            /****************************************/
            // White
            bitboard = (uint64_t)0;
            bitboard = setBit(bitboard, i); 
            attacks = (uint64_t)0;
            attacks |= ((bitboard & notColumnA) << 7);  // Nord-Ovest
            attacks |= ((bitboard & notColumnH) << 9);  // Nord-Est
            pawnAttacks[Color::WHITE][i] = attacks;

            // Black
            bitboard = (uint64_t)0;
            bitboard = setBit(bitboard, i); 
            attacks = (uint64_t)0;
            attacks |= ((bitboard & notColumnH) >> 7);  // Sud-Est
            attacks |= ((bitboard & notColumnA) >> 9);  // Sud-Ovest
            pawnAttacks[Color::BLACK][i] = attacks;
            
        }
    }

}


