#include "MagicBitboards.hpp"
#include "BitOperations.hpp"

#include <iostream>

namespace MagicBitboards {

    uint64_t RookMasks[64]          = {0};
    uint64_t BishopMasks[64]        = {0};
    uint64_t RookAttacks[64][4096]  = {{0}};
    uint64_t BishopAttacks[64][512] = {{0}};
    uint64_t RookMagics[64]         = {0};
    uint64_t BishopMagics[64]       = {0};

    /*****************************************************************************************/
    /*                 BUILD MAGIC BITBOARDS                                                 */
    /*****************************************************************************************/

    uint64_t ComputeRay(int direction, int square, uint64_t totalOccupation) {
        uint64_t attacks = 0ULL;
        int r = square / 8;
        int c = square % 8;

        while (true) {
            if (direction == 8)       { r++; }      // Su
            else if (direction == -8) { r--; }      // Giù
            else if (direction == -1) { c--; }      // Sinistra
            else if (direction == 1)  { c++; }      // Destra
            else if (direction == 7)  { r++; c--; } // Su-Sinistra
            else if (direction == 9)  { r++; c++; } // Su-Destra
            else if (direction == -9) { r--; c--; } // Giù-Sinistra
            else if (direction == -7) { r--; c++; } // Giù-Destra

            if (r < 0 || r > 7 || c < 0 || c > 7) break;

            int targetSquare = r * 8 + c;
            attacks |= (1ULL << targetSquare);

            if ((1ULL << targetSquare) & totalOccupation) break;
        }
        return attacks;
    }

    uint64_t MaskRookAttacks(int square) {
        uint64_t mask = 0ULL;
        int r = square / 8;
        int c = square % 8;

        for (int tr = r + 1; tr <= 6; tr++) mask |= (1ULL << (tr * 8 + c)); // Su
        for (int tr = r - 1; tr >= 1; tr--) mask |= (1ULL << (tr * 8 + c)); // Giù
        for (int tc = c + 1; tc <= 6; tc++) mask |= (1ULL << (r * 8 + tc)); // Destra
        for (int tc = c - 1; tc >= 1; tc--) mask |= (1ULL << (r * 8 + tc)); // Sinistra

        return mask;
    }

    uint64_t MaskBishopAttacks(int square) {
        uint64_t mask = 0ULL;
        int r = square / 8;
        int c = square % 8;

        for (int tr = r + 1, tc = c + 1; tr <= 6 && tc <= 6; tr++, tc++) mask |= (1ULL << (tr * 8 + tc));
        for (int tr = r + 1, tc = c - 1; tr <= 6 && tc >= 1; tr++, tc--) mask |= (1ULL << (tr * 8 + tc));
        for (int tr = r - 1, tc = c + 1; tr >= 1 && tc <= 6; tr--, tc++) mask |= (1ULL << (tr * 8 + tc));
        for (int tr = r - 1, tc = c - 1; tr >= 1 && tc >= 1; tr--, tc--) mask |= (1ULL << (tr * 8 + tc));

        return mask;
    }

    uint64_t SetOccupancy(int index, int bitsInMask, uint64_t attackMask) {
        uint64_t occupancy = 0ULL;
        for (int i = 0; i < bitsInMask; i++) {
            int square = __builtin_ctzll(attackMask);
            attackMask = clearBit(attackMask, square);

            if (index & (1 << i)) occupancy |= (1ULL << square);
        }

        return occupancy;
    }

    template <size_t N>
    static inline uint64_t MagicLookup(int square, uint64_t occupancy,
                                        const uint64_t* masks, const uint64_t* magics,
                                        const uint64_t (*table)[N]) {
        uint64_t blockers = occupancy & masks[square];
        int shift = 64 - __builtin_popcountll(masks[square]);
        uint64_t index = (blockers * magics[square]) >> shift;
        return table[square][index];
    }

    uint64_t GetRookAttacks(int square, uint64_t occupancy) {
        return MagicLookup<4096>(square, occupancy, RookMasks, RookMagics, RookAttacks);
    }
    uint64_t GetBishopAttacks(int square, uint64_t occupancy) {
        return MagicLookup<512>(square, occupancy, BishopMasks, BishopMagics, BishopAttacks);
    }

    uint64_t random_uint64() {
        static uint64_t seed = 1070372ULL;
        seed ^= seed >> 12;
        seed ^= seed << 25;
        seed ^= seed >> 27;
        return seed * 2685821657736338717ULL;
    }

    uint64_t random_uint64_fewbits() {
        return random_uint64() & random_uint64() & random_uint64();
    }

    uint64_t FindMagic(int square, int bits, bool isRook) {
        uint64_t mask = isRook ? MaskRookAttacks(square) : MaskBishopAttacks(square);
        int variations = 1 << bits;

        uint64_t occupancies[4096];
        uint64_t attacks[4096];
        uint64_t usedAttacks[4096];

        // 1. Precalcola tutte le occupazioni e i veri attacchi
        for (int i = 0; i < variations; i++) {
            occupancies[i] = SetOccupancy(i, bits, mask);
            attacks[i] = 0ULL;
            if (isRook) {
                attacks[i] |= ComputeRay(8, square, occupancies[i]);
                attacks[i] |= ComputeRay(-8, square, occupancies[i]);
                attacks[i] |= ComputeRay(1, square, occupancies[i]);
                attacks[i] |= ComputeRay(-1, square, occupancies[i]);
            } else {
                attacks[i] |= ComputeRay(9, square, occupancies[i]);
                attacks[i] |= ComputeRay(-9, square, occupancies[i]);
                attacks[i] |= ComputeRay(7, square, occupancies[i]);
                attacks[i] |= ComputeRay(-7, square, occupancies[i]);
            }
        }

        // 2. Prova milioni di numeri casuali finché non trovi quello che non genera collisioni
        for (int k = 0; k < 100000000; k++) {
            uint64_t magic = random_uint64_fewbits();
            if (__builtin_popcountll((mask * magic) & 0xFF00000000000000ULL) < 6) continue;

            for (int i = 0; i < 4096; i++) usedAttacks[i] = 0ULL;
            bool fail = false;

            for (int i = 0; i < variations; i++) {
                // NOTA: (64 - bits) sostituisce completamente le vecchie tabelle RookShifts!
                uint64_t magicIndex = (occupancies[i] * magic) >> (64 - bits);

                if (usedAttacks[magicIndex] == 0ULL) {
                    usedAttacks[magicIndex] = attacks[i];
                } else if (usedAttacks[magicIndex] != attacks[i]) {
                    fail = true; // Collisione trovata, il numero si scarta!
                    break;
                }
            }
            if (!fail) return magic; // TROVATO!
        }
        std::cout << "Magia fallita sulla casa " << square << "\n";
        return 0ULL;
    }

    void InitMagicBitboards() {
        for (int square = 0; square < 64; square++) {
            RookMasks[square] = MaskRookAttacks(square);
            BishopMasks[square] = MaskBishopAttacks(square);

            int rookBits = __builtin_popcountll(RookMasks[square]);
            int bishopBits = __builtin_popcountll(BishopMasks[square]);

            RookMagics[square] = FindMagic(square, rookBits, true);
            BishopMagics[square] = FindMagic(square, bishopBits, false);

            int rookOccupancyVariations = 1 << rookBits;
            int bishopOccupancyVariations = 1 << bishopBits;

            for (int i = 0; i < rookOccupancyVariations; i++) {
                uint64_t blockers = SetOccupancy(i, rookBits, RookMasks[square]);

                uint64_t attacks = 0ULL;
                attacks |= ComputeRay(-1, square, blockers);
                attacks |= ComputeRay(1, square, blockers);
                attacks |= ComputeRay(8, square, blockers);
                attacks |= ComputeRay(-8, square, blockers);

                // NOTA: Usiamo (64 - rookBits)
                uint64_t magicIndex = (blockers * RookMagics[square]) >> (64 - rookBits);
                RookAttacks[square][magicIndex] = attacks;
            }

            for (int i = 0; i < bishopOccupancyVariations; i++) {
                uint64_t blockers = SetOccupancy(i, bishopBits, BishopMasks[square]);

                uint64_t attacks = 0ULL;
                attacks |= ComputeRay(9, square, blockers);
                attacks |= ComputeRay(7, square, blockers);
                attacks |= ComputeRay(-9, square, blockers);
                attacks |= ComputeRay(-7, square, blockers);

                uint64_t magicIndex = (blockers * BishopMagics[square]) >> (64 - bishopBits);
                BishopAttacks[square][magicIndex] = attacks;
            }
        }
    }
}
