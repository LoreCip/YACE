# ==============================================================================
# YACE GLOBAL MAKEFILE (Unified Build System - Phase 2 Architecture)
# ==============================================================================

# Nomi degli eseguibili finali (tutti posizionati nella cartella build globale)
TARGET_GAME = build/main_game
TARGET_FEN  = build/main_fen
TARGET_NNUE = build/main_nnue

# Compilatore, inclusioni e flag unificati
CXX      = g++
INCLUDES = -Isrc/Core/include \
		   -Isrc/Core/LookupTables/include \
           -Isrc/Evaluation/include \
           -Isrc/Evaluation/include/NNUE \
           -Isrc/Search/include

CXXFLAGS = -Wall -Wextra -std=c++17 -O3 -flto -fopenmp $(INCLUDES)
LDFLAGS  = -lcblas -lblas

# Cartelle sorgenti radice
APP_SRC_DIR       = src/App
CORE_SRC_DIR      = src/Core/src
LOOK_SRC_DIR      = src/Core/LookupTables/src
EVAL_SRC_DIR      = src/Evaluation/src
EVAL_NNUE_SRC_DIR = src/Evaluation/src/NNUE
SEARCH_SRC_DIR    = src/Search/src

# Cartelle di build centralizzate
OBJ_DIR           = build/objects
OBJ_APP_DIR       = $(OBJ_DIR)/App
OBJ_CORE_DIR      = $(OBJ_DIR)/Core
OBJ_LOOK_DIR      = $(OBJ_CORE_DIR)/LookupTables
OBJ_EVAL_DIR      = $(OBJ_DIR)/Evaluation
OBJ_EVAL_NNUE_DIR = $(OBJ_DIR)/Evaluation/NNUE
OBJ_SEARCH_DIR    = $(OBJ_DIR)/Search

# ------------------------------------------------------------------------------
# 1. SCANSIONE SORGENTI E MAPPARE I FILE OGGETTO
# ------------------------------------------------------------------------------

# Trova tutti i file sorgente (esclusi i main)
CORE_SRCS      = $(wildcard $(CORE_SRC_DIR)/*.cpp)
LOOK_SRCS      = $(wildcard $(LOOK_SRC_DIR)/*.cpp)
EVAL_SRCS      = $(wildcard $(EVAL_SRC_DIR)/*.cpp)
EVAL_NNUE_SRCS = $(wildcard $(EVAL_NNUE_SRC_DIR)/*.cpp)
SEARCH_SRCS    = $(wildcard $(SEARCH_SRC_DIR)/*.cpp)

# Crea i percorsi dei file oggetto corrispondenti
CORE_OBJS      = $(CORE_SRCS:$(CORE_SRC_DIR)/%.cpp=$(OBJ_CORE_DIR)/%.o)
LOOK_OBJS      = $(LOOK_SRCS:$(LOOK_SRC_DIR)/%.cpp=$(OBJ_LOOK_DIR)/%.o)
EVAL_OBJS      = $(EVAL_SRCS:$(EVAL_SRC_DIR)/%.cpp=$(OBJ_EVAL_DIR)/%.o)
EVAL_NNUE_OBJS = $(EVAL_NNUE_SRCS:$(EVAL_NNUE_SRC_DIR)/%.cpp=$(OBJ_EVAL_NNUE_DIR)/%.o)
SEARCH_OBJS    = $(SEARCH_SRCS:$(SEARCH_SRC_DIR)/%.cpp=$(OBJ_SEARCH_DIR)/%.o)

# Insieme globale di tutti gli oggetti Core/Libreria del motore
ALL_CORE_OBJS  = $(CORE_OBJS) $(LOOK_OBJS) $(EVAL_OBJS) $(EVAL_NNUE_OBJS) $(SEARCH_OBJS)

# Oggetti specifici per i punti di ingresso (I vari Main)
OBJ_MAIN_GAME  = $(OBJ_APP_DIR)/main_game.o
OBJ_MAIN_FEN   = $(OBJ_APP_DIR)/main_fen.o
OBJ_MAIN_NNUE  = $(OBJ_APP_DIR)/main_nnue.o

# ------------------------------------------------------------------------------
# 2. REGOLE DI COMPILAZIONE TARGETS
# ------------------------------------------------------------------------------

# Regola di default: compila tutti e tre gli eseguibili
all: $(TARGET_GAME) $(TARGET_FEN) $(TARGET_NNUE)

# Build di main_game
$(TARGET_GAME): $(ALL_CORE_OBJS) $(OBJ_MAIN_GAME)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(ALL_CORE_OBJS) $(OBJ_MAIN_GAME) -o $(TARGET_GAME) $(LDFLAGS)
	@echo "[SUCCESS] Build completata: $(TARGET_GAME)"

# Build di main_fen
$(TARGET_FEN): $(ALL_CORE_OBJS) $(OBJ_MAIN_FEN)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(ALL_CORE_OBJS) $(OBJ_MAIN_FEN) -o $(TARGET_FEN) $(LDFLAGS)
	@echo "[SUCCESS] Build completata: $(TARGET_FEN)"

# Build di main_nnue
$(TARGET_NNUE): $(ALL_CORE_OBJS) $(OBJ_MAIN_NNUE)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(ALL_CORE_OBJS) $(OBJ_MAIN_NNUE) -o $(TARGET_NNUE) $(LDFLAGS)
	@echo "[SUCCESS] Build completata: $(TARGET_NNUE)"

# ------------------------------------------------------------------------------
# 3. REGOLE GENERICHE PER LA COMPILAZIONE DEI FILE OGGETTO
# ------------------------------------------------------------------------------

# Compilazione App (I Main)
$(OBJ_APP_DIR)/%.o: $(APP_SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_APP_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compilazione Core
$(OBJ_CORE_DIR)/%.o: $(CORE_SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_CORE_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compilazione Lookup
$(OBJ_LOOK_DIR)/%.o: $(LOOK_SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_LOOK_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@


# Compilazione Evaluation (Classica)
$(OBJ_EVAL_DIR)/%.o: $(EVAL_SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_EVAL_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compilazione Evaluation (NNUE)
$(OBJ_EVAL_NNUE_DIR)/%.o: $(EVAL_NNUE_SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_EVAL_NNUE_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compilazione Search
$(OBJ_SEARCH_DIR)/%.o: $(SEARCH_SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_SEARCH_DIR)
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
	@mkdir -p gauntlet/versions/v$(V)/src
	@cp -r src/* gauntlet/versions/v$(V)/src/
	@echo "Compressione di tutti i sorgenti unificati in v$(V).tar.gz..."
	@tar -czf gauntlet/versions/v$(V).tar.gz -C gauntlet/versions v$(V)
	@rm -rf gauntlet/versions/v$(V)
	@echo "Salvataggio degli eseguibili di produzione..."
	@cp build/main_game gauntlet/versions/engine_v$(V)
	@cp build/main_fen gauntlet/versions/engine_fen_v$(V)
	@echo "[RELEASE] Versione v$(V) rilasciata con successo in gauntlet/versions/"

.PHONY: all clean release