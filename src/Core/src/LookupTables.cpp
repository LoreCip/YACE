#include "LookupTables.hpp"
#include "BitOperations.hpp"

#include <iostream>
#include <string>

namespace LookupTables {

    const uint64_t notColumnA       = 0xFEFEFEFEFEFEFEFEULL;
    const uint64_t notColumnB       = 0xFDFDFDFDFDFDFDFDULL;
    const uint64_t notColumnG       = 0xBFBFBFBFBFBFBFBFULL;
    const uint64_t notColumnH       = 0x7F7F7F7F7F7F7F7FULL;
    const uint64_t notColumnADouble = 0xFCFCFCFCFCFCFCFCULL;
    const uint64_t notColumnHDouble = 0x3F3F3F3F3F3F3F3FULL;
    const uint64_t nullEdge         = 0xFFFFFFFFFFFFFFFFULL;

    const uint64_t maskWK = 0x60ULL;                 // Squares f1, g1
    const uint64_t maskWQ = 0xEULL;                  // Squares b1, c1, d1
    const uint64_t maskBK = 0x6000000000000000ULL;   // Squares f8, g8
    const uint64_t maskBQ = 0x0E00000000000000ULL;   // Squares b8, c8, d8

    uint64_t realKnightAttacks[64] = {0};
    uint64_t realKingAttacks[64]   = {0};
    uint64_t realPawnAttacks[2][64] = {{0}};

    uint64_t realCastlingKeys[16] = {0};
    uint64_t realEnPassantKeys[65] = {0};
    uint64_t realPieceKeys[2][6][64] = {{{0}}};
    uint64_t realSideKey = {0};

    uint8_t realCastlingMasks[64] = {
        13, 15, 15, 15, 12, 15, 15, 14, // 0-7   (White: rook A1, king, rook H1)
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15,
         7, 15, 15, 15,  3, 15, 15, 11  // 56-63 (Black: rook A8, king, rook H8)
    };

    const uint64_t (&knightAttacks)[64]   = realKnightAttacks;
    const uint64_t (&kingAttacks)[64]     = realKingAttacks;
    const uint64_t (&pawnAttacks)[2][64]  = realPawnAttacks;
    const uint8_t  (&castlingMasks)[64]   = realCastlingMasks;
    const uint64_t (&castlingKeys)[16]    = realCastlingKeys;
    const uint64_t (&enPassantKeys)[65]   = realEnPassantKeys;
    const uint64_t (&pieceKeys)[2][6][64] = realPieceKeys;
    const uint64_t &sideKey               = realSideKey;
    int pieceValues[6] = {100, 500, 300, 300, 900, 0};
    int pstTables[6][64] = {{0}};

    void init(const std::string& configFile) {

        std::ifstream file(configFile);
        if (!file.is_open()) {
            std::cerr << "error cannot open file " << configFile << "\n";
        } else {
            
            if (!readArrayBlock(file, pieceValues, 6)) {
                std::cerr << "error failed loading of pieceValues array\n";
            }

            for (int p = 0; p < 6; p++) {
                if (!readArrayBlock(file, pstTables[p], 64)) {
                    std::cerr << "error failed loading of evaluation table " << p << "\n";
                }
            }
            
            file.close();
            std::cout << "info evaluation tables loaded\n";
        }

        // --- ZOBRIST KEYS INIT ---
        uint64_t pseudoRandom = 1070372ULL;
        auto rand64 = [&]() -> uint64_t {
            pseudoRandom ^= pseudoRandom >> 12;
            pseudoRandom ^= pseudoRandom << 25;
            pseudoRandom ^= pseudoRandom >> 27;
            return pseudoRandom * 2685821657736338717ULL;
        };

        for (int i = 0; i < 16; i++) realCastlingKeys[i] = rand64();
        for (int i = 0; i < 65; i++) realEnPassantKeys[i] = rand64();

        for (int c = 0; c < 2; c++) {
            for (int p = 0; p < 6; p++) {
                for (int s = 0; s < 64; s++) {
                    realPieceKeys[c][p][s] = rand64();
                }
            }
        }
        realSideKey = rand64();

        // Attack matrix
        for (int i = 0; i < 64; i++) {   
            
            // 1. Knights
            uint64_t bitboard = 0ULL;
            bitboard = setBit(bitboard, i); 
            uint64_t attacks = 0ULL;

            attacks |= ((bitboard & notColumnH) << 17);        
            attacks |= ((bitboard & notColumnHDouble) << 10);  
            attacks |= ((bitboard & notColumnHDouble) >> 6);   
            attacks |= ((bitboard & notColumnH) >> 15);        

            attacks |= ((bitboard & notColumnA) >> 17);        
            attacks |= ((bitboard & notColumnADouble) >> 10);  
            attacks |= ((bitboard & notColumnADouble) << 6);   
            attacks |= ((bitboard & notColumnA) << 15);        

            realKnightAttacks[i] = attacks;
            
            // 2. King
            bitboard = 0ULL;
            bitboard = setBit(bitboard, i); 
            attacks = 0ULL;

            attacks |= ((bitboard & notColumnA) >> 1);  
            attacks |= ((bitboard & notColumnH) << 1);  
            attacks |= (bitboard << 8);                 
            attacks |= (bitboard >> 8);                 

            attacks |= ((bitboard & notColumnH) << 9);  
            attacks |= ((bitboard & notColumnA) << 7);  
            attacks |= ((bitboard & notColumnH) >> 7);  
            attacks |= ((bitboard & notColumnA) >> 9);  

            realKingAttacks[i] = attacks;

            // 3. Pawn Captures
            // White
            bitboard = 0ULL;
            bitboard = setBit(bitboard, i); 
            attacks = 0ULL;
            attacks |= ((bitboard & notColumnA) << 7);  
            attacks |= ((bitboard & notColumnH) << 9);  
            realPawnAttacks[0][i] = attacks;

            // Black
            bitboard = 0ULL;
            bitboard = setBit(bitboard, i); 
            attacks = 0ULL;
            attacks |= ((bitboard & notColumnH) >> 7);  
            attacks |= ((bitboard & notColumnA) >> 9);  
            realPawnAttacks[1][i] = attacks;
        }
    }


    bool readArrayBlock(std::ifstream& file, int* targetArray, int expectedSize) {
        std::string line;
        std::string blockName = "";

        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            if (line[0] == '#') {
                blockName = line;
                break;
            }
        }
        if (blockName.empty()) return false;

        int size = 0;
        if (!(file >> size)) {
            std::cerr << "[ERROR] Impossibile leggere la dimensione nel blocco: " << blockName << "\n";
            return false;
        }

        if (size != expectedSize) {
            std::cerr << "[ERROR] Discrepanza sul blocco " << blockName 
                      << " -> Attesi: " << expectedSize << ", Trovati sul file: " << size << "\n";
            return false;
        }

        for (int i = 0; i < size; i++) {
            if (!(file >> targetArray[i])) {
                std::cerr << "[ERROR] Dati insufficienti o corrotti nel blocco: " << blockName 
                          << " (Errore all'elemento " << i << ")\n";
                return false;
            }
        }

        std::string dummy;
        std::getline(file, dummy);

        return true;
    }
}