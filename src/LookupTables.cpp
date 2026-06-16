#include "BitOperations.hpp"


namespace LookupTables {

    uint64_t knightAttacks[64] = {(uint64_t)0};
    uint64_t kingAttacks[64]= {(uint64_t)0};;

    // Edge bitboard
    const uint64_t illegalEast = 0x7F7F7F7F7F7F7F7F; // 01111111 ....
    const uint64_t illegalWest = 0xFEFEFEFEFEFEFEFE; // 11111110 ....
    const uint64_t illegalDoubleEast = 0x3F3F3F3F3F3F3F3F; // 00111111 ....
    const uint64_t illegalDoubleWest = 0xFCFCFCFCFCFCFCFC; // 11111100 ....
    
    void init(){
        for (int i = 0; i < 64; i++){
            
            // Compute the knight moves
            // A. Create empty board
            uint64_t bitboard = (uint64_t)0;
            bitboard = setBit(bitboard, i); 

            // B. Number of squares that can be attacked
            uint64_t attacks = (uint64_t)0;

            // La formula è sempre: (Traslazione) AND (Maschera Bordo)
            attacks |= (bitboard << 17) & illegalWest;   // Nord-Nord-Est (+2 righe, +1 colonna)
            attacks |= (bitboard << 10) & illegalDoubleWest;  // Nord-Est-Est (+1 riga,  +2 colonne)
            attacks |= (bitboard >> 6)  & illegalDoubleWest;  // Sud-Est-Est  (-1 riga,  +2 colonne)
            attacks |= (bitboard >> 15) & illegalWest;   // Sud-Sud-Est  (-2 righe, +1 colonna)
            
            attacks |= (bitboard >> 17) & illegalEast;   // Sud-Sud-Ovest (-2 righe, -1 colonna)
            attacks |= (bitboard >> 10) & illegalDoubleEast;  // Sud-Ovest-Ovest (-1 riga, -2 colonne)
            attacks |= (bitboard << 6)  & illegalDoubleEast;  // Nord-Ovest-Ovest (+1 riga, -2 colonne)
            attacks |= (bitboard << 15) & illegalEast;   // Nord-Nord-Ovest (+2 righe, -1 colonna)

            // D. Salviamo la bitboard finale calcolata nella Lookup Table
            knightAttacks[square] = attacks;
            

            // Compute the king moves
            // A. Create empty board
            bitboard = (uint64_t)0;
            bitboard = setBit(bitboard, i); 

            // B. Number of squares that can be attacked
            attacks = (uint64_t)0;

            // La formula è sempre: (Traslazione) AND (Maschera Bordo)
            attacks |= (bitboard >> 1) & illegalEast;  // Ovest (-1)
            attacks |= (bitboard << 1) & illegalWest;  // Est (+1)
            attacks |= (bitboard << 8);                // Nord (+8, No mask)
            attacks |= (bitboard >> 8);                // Sud (-8,  No mask)
            
            // Movimenti Diagonali
            attacks |= (bitboard << 9) & illegalWest;  // Nord-Est (+8 +1)
            attacks |= (bitboard << 7) & illegalEast;  // Nord-Ovest (+8 -1)
            attacks |= (bitboard >> 7) & illegalWest;  // Sud-Est (-8 +1)
            attacks |= (bitboard >> 9) & illegalEast;  // Sud-Ovest (-8 -1)
            // D. Salviamo la bitboard finale calcolata nella Lookup Table
            kingAttacks[i] = attacks;

        }
    }

}


