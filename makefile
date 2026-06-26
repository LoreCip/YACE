# ==============================================================================
# YACE GLOBAL MAKEFILE (Unified Build System)
# ==============================================================================

# Nomi degli eseguibili finali (tutti posizionati nella cartella build globale)
TARGET_GAME = build/main_game
TARGET_FEN  = build/main_fen
TARGET_NNUE = build/main_nnue

# Compilatore e flag unificati
CXX      = g++ -D_DEBUG
CXXFLAGS = -Wall -Wextra -std=c++17 -O3 -fopenmp -flto \
           -IChessEngine/include -INNUE/include
LDFLAGS  = -lcblas -lblas

# Cartelle sorgenti radice
ENGINE_SRC_DIR = ChessEngine/src
NNUE_SRC_DIR   = NNUE/src

# Cartelle di build centralizzate
OBJ_DIR        = build/objects
OBJ_ENGINE_DIR = $(OBJ_DIR)/engine
OBJ_NNUE_DIR   = $(OBJ_DIR)/nnue

# ------------------------------------------------------------------------------
# 1. SCANSIONE SORGENTI E MAPPARE I FILE OGGETTO
# ------------------------------------------------------------------------------

# CHESS ENGINE: Isola i file core (.cpp) escludendo i vari file main_*.cpp
ALL_ENGINE_SRCS   = $(wildcard $(ENGINE_SRC_DIR)/*.cpp)
CORE_ENGINE_SRCS  = $(filter-out $(ENGINE_SRC_DIR)/main%, $(ALL_ENGINE_SRCS))
CORE_ENGINE_OBJS  = $(CORE_ENGINE_SRCS:$(ENGINE_SRC_DIR)/%.cpp=$(OBJ_ENGINE_DIR)/%.o)

# NNUE: Isola i file core (.cpp) escludendo main_nnue.cpp
ALL_NNUE_SRCS     = $(wildcard $(NNUE_SRC_DIR)/*.cpp)
CORE_NNUE_SRCS    = $(filter-out $(NNUE_SRC_DIR)/main%, $(ALL_NNUE_SRCS))
CORE_NNUE_OBJS    = $(CORE_NNUE_SRCS:$(NNUE_SRC_DIR)/%.cpp=$(OBJ_NNUE_DIR)/%.o)

# Insieme globale di tutti gli oggetti Core del motore (Engine + NNUE integrata)
ALL_CORE_OBJS     = $(CORE_ENGINE_OBJS) $(CORE_NNUE_OBJS)

# Oggetti specifici per i punti di ingresso (I vari Main)
OBJ_MAIN_GAME     = $(OBJ_ENGINE_DIR)/main_game.o
OBJ_MAIN_FEN      = $(OBJ_ENGINE_DIR)/main_fen.o
OBJ_MAIN_NNUE     = $(OBJ_NNUE_DIR)/main_nnue.o

# ------------------------------------------------------------------------------
# 2. REGOLE DI COMPILAZIONE TARGETS
# ------------------------------------------------------------------------------

# Regola di default: compila tutti e tre gli eseguibili
all: $(TARGET_GAME) $(TARGET_FEN) $(TARGET_NNUE)

# Build di main_game (Engine + NNUE + il suo main_game specifico)
$(TARGET_GAME): $(ALL_CORE_OBJS) $(OBJ_MAIN_GAME)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(ALL_CORE_OBJS) $(OBJ_MAIN_GAME) -o $(TARGET_GAME) $(LDFLAGS)
	@echo "[SUCCESS] Build completata: $(TARGET_GAME)"

# Build di main_fen (Engine + NNUE + il suo main_fen specifico)
$(TARGET_FEN): $(ALL_CORE_OBJS) $(OBJ_MAIN_FEN)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(ALL_CORE_OBJS) $(OBJ_MAIN_FEN) -o $(TARGET_FEN) $(LDFLAGS)
	@echo "[SUCCESS] Build completata: $(TARGET_FEN)"

# Build di main_nnue (Eseguibile autonomo/test per il modulo NNUE)
$(TARGET_NNUE): $(ALL_CORE_OBJS) $(OBJ_MAIN_NNUE)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(ALL_CORE_OBJS) $(OBJ_MAIN_NNUE) -o $(TARGET_NNUE) $(LDFLAGS)
	@echo "[SUCCESS] Build completata: $(TARGET_NNUE)"

# ------------------------------------------------------------------------------
# 3. REGOLE GENERICHE PER LA COMPILAZIONE DEI FILE OGGETTO
# ------------------------------------------------------------------------------

# Compilazione dei sorgenti ChessEngine -> build/objects/engine/
$(OBJ_ENGINE_DIR)/%.o: $(ENGINE_SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_ENGINE_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compilazione dei sorgenti NNUE -> build/objects/nnue/
$(OBJ_NNUE_DIR)/%.o: $(NNUE_SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_NNUE_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ------------------------------------------------------------------------------
# 4. UTILITY E PACCHETTI DI RELEASE
# ------------------------------------------------------------------------------

clean:
	rm -rf build
	@echo "[CLEAN] Cartella build eliminata correttamente."

# Uso: make release V=1.0
release: all
	@if [ -z "$(V)" ]; then \
		echo "Errore: Devi specificare una versione. Usa 'make release V=1.0'"; \
		exit 1; \
	fi
	@echo "Creazione dello snapshot per la versione v$(V)..."
	@echo "v$(V)" > build/version.txt
	@mkdir -p gauntlet/versions/v$(V)/ChessEngine
	@mkdir -p gauntlet/versions/v$(V)/NNUE
	@cp -r ChessEngine/src ChessEngine/include gauntlet/versions/v$(V)/ChessEngine/
	@cp -r NNUE/src NNUE/include gauntlet/versions/v$(V)/NNUE/
	@echo "Compressione di tutti i sorgenti unificati in v$(V).tar.gz..."
	@tar -czf gauntlet/versions/v$(V).tar.gz -C gauntlet/versions v$(V)
	@rm -rf gauntlet/versions/v$(V)
	@echo "Salvataggio degli eseguibili di produzione..."
	@cp build/main_game gauntlet/versions/engine_v$(V)
	@cp build/main_fen gauntlet/versions/engine_fen_v$(V)
	@echo "[RELEASE] Versione v$(V) rilasciata con successo in gauntlet/versions/"

.PHONY: all clean release