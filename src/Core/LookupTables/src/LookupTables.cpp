#include "LookupTables.hpp"

namespace LookupTables {

    const uint64_t (&knightAttacks)[64]  = AttackTables::knightAttacks;
    const uint64_t (&kingAttacks)[64]    = AttackTables::kingAttacks;
    const uint64_t (&pawnAttacks)[2][64] = AttackTables::pawnAttacks;

    const uint64_t (&castlingKeys)[16]    = Zobrist::castlingKeys;
    const uint64_t (&enPassantKeys)[65]   = Zobrist::enPassantKeys;
    const uint64_t (&pieceKeys)[2][6][64] = Zobrist::pieceKeys;
    const uint64_t &sideKey               = Zobrist::sideKey;

    void init(const std::string& configFile) {
        Evaluation::LoadFromFile(configFile);
        Zobrist::Init();
        AttackTables::Init();
        InitMagicBitboards();
    }
}
