#include "LookupTables.hpp"

namespace LookupTables {
    void init() {
        Zobrist::Init();
        AttackTables::Init();
        MagicBitboards::InitMagicBitboards();
    }
}
