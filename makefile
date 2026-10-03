# ==============================================================================
# YACE MAKEFILE
# ==============================================================================

TARGET_GAME = build/main_game
TARGET_FEN  = build/main_fen

CXX       = g++
INCLUDES  = -Isrc/Core/include \
            -Isrc/Core/LookupTables/include \
            -Isrc/Evaluation/include \
            -Isrc/Evaluation/include/NNUE \
            -Isrc/Search/include
PGO_FLAGS =
CXXFLAGS  = -Wall -Wextra -std=c++17 -O3 -flto -march=native -DNDEBUG \
            -mavx512f -mavx512bw -mavx512dq -mavx512vl -mavx512vnni \
            -mprefer-vector-width=512 $(INCLUDES) $(PGO_FLAGS)
LDFLAGS   = $(PGO_FLAGS)

# Oggetti del motore (tutto tranne i main), specchiati sotto build/objects
OBJ_DIR     = build/objects
ENGINE_SRCS = $(wildcard src/Core/src/*.cpp src/Core/LookupTables/src/*.cpp \
                         src/Evaluation/src/*.cpp src/Evaluation/src/NNUE/*.cpp \
                         src/Search/src/*.cpp)
ENGINE_OBJS = $(ENGINE_SRCS:src/%.cpp=$(OBJ_DIR)/%.o)

all: $(TARGET_GAME) $(TARGET_FEN)

$(TARGET_GAME): $(ENGINE_OBJS) $(OBJ_DIR)/App/main_game.o
$(TARGET_FEN):  $(ENGINE_OBJS) $(OBJ_DIR)/App/main_fen.o

# Solo il perft (main_fen) usa OpenMP
$(TARGET_FEN) $(OBJ_DIR)/App/main_fen.o: private CXXFLAGS += -fopenmp
$(TARGET_FEN): private LDFLAGS += -fopenmp

$(TARGET_GAME) $(TARGET_FEN):
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)
	@echo "[SUCCESS] Build completata: $@"

# -MMD -MP: ricompila anche quando cambia un header
$(OBJ_DIR)/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

-include $(ENGINE_OBJS:.o=.d) $(OBJ_DIR)/App/main_game.d $(OBJ_DIR)/App/main_fen.d

clean:
	rm -rf build
	@echo "[CLEAN] Cartella build eliminata correttamente."

# Uso: make release V=1.0
release: pgo
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

PGO_DIR = build/pgo-data
pgo:
	@echo "[PGO] Fase 1/3: build strumentata..."
	@rm -rf $(PGO_DIR)
	@mkdir -p $(PGO_DIR)
	$(MAKE) clean
	$(MAKE) PGO_FLAGS="-fprofile-generate=$(PGO_DIR)" all
	@echo "[PGO] Fase 2/3: esecuzione del carico di lavoro rappresentativo (bench, NNUE e HCE)..."
	@printf 'bench 9\nquit\n' | ./build/main_game -e nnue -w parameters/weights/nnue_weights_v3-6.bin -p parameters/evals.txt -c 1 > /dev/null
	@printf 'bench 9\nquit\n' | ./build/main_game -e hce -p parameters/evals.txt -c 1 > /dev/null
	@echo "[PGO] Fase 3/3: rebuild ottimizzata con i dati di profilo raccolti..."
	$(MAKE) clean
	$(MAKE) PGO_FLAGS="-fprofile-use=$(PGO_DIR) -fprofile-correction -Wno-missing-profile" all
	@echo "[PGO] Completato: build/main_game e build/main_fen ottimizzati con PGO."

.PHONY: all clean release pgo