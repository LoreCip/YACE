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
            attacks |= (bitboard << 17) & notColumnH;        // Nord-Nord-Est (+2 righe, +1 colonna)
            attacks |= (bitboard << 10) & notColumnHDouble;  // Nord-Est-Est (+1 riga,  +2 colonne)
            attacks |= (bitboard >> 6)  & notColumnHDouble;  // Sud-Est-Est  (-1 riga,  +2 colonne)
            attacks |= (bitboard >> 15) & notColumnH;        // Sud-Sud-Est  (-2 righe, +1 colonna)
            
            attacks |= (bitboard >> 17) & notColumnA;        // Sud-Sud-Ovest (-2 righe, -1 colonna)
            attacks |= (bitboard >> 10) & notColumnADouble;  // Sud-Ovest-Ovest (-1 riga, -2 colonne)
            attacks |= (bitboard << 6)  & notColumnADouble;  // Nord-Ovest-Ovest (+1 riga, -2 colonne)
            attacks |= (bitboard << 15) & notColumnA;        // Nord-Nord-Ovest (+2 righe, -1 colonna)

            knightAttacks[i] = attacks;
            

            /****************************************/
            /*       Compute the king moves         */
            /****************************************/
            bitboard = (uint64_t)0;
            bitboard = setBit(bitboard, i); 

            attacks = (uint64_t)0;

            attacks |= (bitboard >> 1) & notColumnA;  // Ovest (-1)
            attacks |= (bitboard << 1) & notColumnH;  // Est (+1)
            attacks |= (bitboard << 8);                // Nord (+8, No mask)
            attacks |= (bitboard >> 8);                // Sud (-8,  No mask)
            
            attacks |= (bitboard << 9) & notColumnH;  // Nord-Est (+8 +1)
            attacks |= (bitboard << 7) & notColumnA;  // Nord-Ovest (+8 -1)
            attacks |= (bitboard >> 7) & notColumnH;  // Sud-Est (-8 +1)
            attacks |= (bitboard >> 9) & notColumnA;  // Sud-Ovest (-8 -1)

            kingAttacks[i] = attacks;

            /****************************************/
            /*       Compute the pawn captures      */
            /****************************************/
            // White
            bitboard = (uint64_t)0;
            bitboard = setBit(bitboard, i); 

            attacks = (uint64_t)0;
            attacks |= (bitboard << 7) & notColumnA;  // Nord-Ovest
            attacks |= (bitboard << 9) & notColumnH;  // Nord-Est

            pawnAttacks[Color::WHITE][i] = attacks;
            
            // Black
            bitboard = (uint64_t)0;
            bitboard = setBit(bitboard, i); 

            attacks = (uint64_t)0;
            attacks |= (bitboard >> 7) & notColumnH;  // Sud-Est
            attacks |= (bitboard >> 9) & notColumnA;  // Sud-Ovest

            pawnAttacks[Color::BLACK][i] = attacks;
            
        }
    }

}


