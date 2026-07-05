#include "Evaluation.hpp"

#include <iostream>

namespace Evaluation {

    int pieceValues[6]     = {100, 500, 300, 300, 900, 0};
    int pstTables[6][64]   = {{0}};

    void LoadFromFile(const std::string& configFile) {
        std::ifstream file(configFile);
        if (!file.is_open()) {
            std::cerr << "error cannot open file " << configFile << "\n";
            return;
        }

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
