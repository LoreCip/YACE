#ifndef LOOKUPTABLES_HPP
#define LOOKUPTABLES_HPP

#include <cstdint>
#include <fstream>
#include <string>

#include "AttackTables.hpp"
#include "Zobrist.hpp"
#include "Evaluation.hpp"
#include "MagicBitboards.hpp"

namespace LookupTables {

    // --- Maschere di colonna / caselle di arrocco (AttackTables) ---
    using AttackTables::notColumnA;
    using AttackTables::notColumnB;
    using AttackTables::notColumnG;
    using AttackTables::notColumnH;
    using AttackTables::notColumnADouble;
    using AttackTables::notColumnHDouble;
    using AttackTables::nullEdge;

    using AttackTables::maskWK;
    using AttackTables::maskWQ;
    using AttackTables::maskBK;
    using AttackTables::maskBQ;

    using AttackTables::castlingMasks;

    extern const uint64_t (&knightAttacks)[64];
    extern const uint64_t (&kingAttacks)[64];
    extern const uint64_t (&pawnAttacks)[2][64];

    // --- ZOBRIST KEYS (Zobrist) ---
    extern const uint64_t (&castlingKeys)[16];
    extern const uint64_t (&enPassantKeys)[65];
    extern const uint64_t (&pieceKeys)[2][6][64];
    extern const uint64_t &sideKey;

    // --- PIECE-SQUARE TABLES / VALUTAZIONE (Evaluation) ---
    using Evaluation::pstTables;
    using Evaluation::pieceValues;
    using Evaluation::readArrayBlock;

    // --- MAGIC BITBOARDS (MagicBitboards) ---
    using MagicBitboards::RookMagics;
    using MagicBitboards::BishopMagics;
    using MagicBitboards::RookMasks;
    using MagicBitboards::BishopMasks;
    using MagicBitboards::RookAttacks;
    using MagicBitboards::BishopAttacks;

    using MagicBitboards::ComputeRay;
    using MagicBitboards::MaskRookAttacks;
    using MagicBitboards::MaskBishopAttacks;
    using MagicBitboards::SetOccupancy;

    using MagicBitboards::GetRookAttacks;
    using MagicBitboards::GetBishopAttacks;
    using MagicBitboards::InitMagicBitboards;

    // Inizializza tutti i moduli
    // valutazione -> Zobrist -> tabelle di attacco -> magic bitboards.
    void init(const std::string& configFile);
}

#endif
