#ifndef EVALUATION_HPP
#define EVALUATION_HPP

#include <fstream>
#include <string>

// Dati di valutazione statica: valore del materiale e piece-square
// tables (PST), caricati da un file di configurazione esterno.
namespace Evaluation {

    extern int pieceValues[6];
    extern int pstTables[6][64];

    void LoadFromFile(const std::string& configFile);
    bool readArrayBlock(std::ifstream& file, int* targetArray, int expectedSize);
}

#endif
