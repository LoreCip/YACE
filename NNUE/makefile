# Nomi degli eseguibili finali
TARGET_MAIN = build/main_nnue

# Compilatore e flag
CXX = g++ -D_DEBUG
CXXFLAGS = -Wall -Wextra -std=c++17 -Iinclude -I/home/lorenzo/Scrivania/Projects/YACE/ChessEngine/include -lblas #-O3 -fopenmp -flto

# Cartelle
SRC_DIR = src
OBJ_DIR = build/objects

# 1. Isoliamo i file comuni dell'engine (tutti i .cpp tranne i vari main*.cpp)
ALL_SRCS   = $(wildcard $(SRC_DIR)/*.cpp)
ENGINE_SRCS = $(filter-out $(SRC_DIR)/main%, $(ALL_SRCS))
ENGINE_OBJS = $(ENGINE_SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# 2. Definiamo i file oggetto specifici per i due punti di ingresso
OBJ_MAIN = $(OBJ_DIR)/main_nnue.o

# Regola di default: compila entrambi gli eseguibili
all: $(TARGET_MAIN)

# Regola per creare l'eseguibile classico (main)
$(TARGET_MAIN): $(ENGINE_OBJS) $(OBJ_MAIN)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(ENGINE_OBJS) $(OBJ_MAIN) -o $(TARGET_MAIN)
	@echo "Build completata: $(TARGET_MAIN)"

# Regola generica per compilare qualsiasi file sorgente .cpp in un file oggetto .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Pulizia dei file compilati
clean:
	rm -rf build

# Uso: make release V=1.0
# release: all
# 	@if [ -z "$(V)" ]; then \
# 		echo "Errore: Devi specificare una versione. Usa 'make release V=1.0'"; \
# 		exit 1; \
# 	fi
# 	@echo "Creazione dello snapshot per la versione v$(V)..."
# 	@echo "v$(V)" > build/version.txt
# 	@echo "Creato file di versione: build/version.txt"
# 	@mkdir -p ../gauntlet/versions/v$(V)/src
# 	@mkdir -p ../gauntlet/versions/v$(V)/include
# 	@cp -r src/* ../gauntlet/versions/v$(V)/src/
# 	@cp -r include/* ../gauntlet/versions/v$(V)/include/
# 	@echo "Compressione dei sorgenti in v$(V).tar.gz in corso..."
# 	@tar -czf ../gauntlet/versions/v$(V).tar.gz -C ../gauntlet/versions v$(V)
# 	@echo "Pulizia della cartella temporanea..."
# 	@rm -rf ../gauntlet/versions/v$(V)
# 	@echo "Salvataggio degli eseguibili..."
# 	@cp build/main_game ../gauntlet/versions/engine_v$(V)
# 	@cp build/main_fen ../gauntlet/versions/engine_fen_v$(V)
# 	@echo "Versione v$(V) rilasciata con successo!"

.PHONY: all clean release