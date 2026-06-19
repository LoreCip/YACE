# Nome dell'eseguibile
TARGET = build/main

# Compilatore e flag
CXX = g++ -D_DEBUG
CXXFLAGS = -Wall -Wextra -std=c++17 -Iinclude

# Cartelle
SRC_DIR = src
OBJ_DIR = build/objects

# Trova tutti i file .cpp nella cartella src
ALL_SRCS = $(wildcard $(SRC_DIR)/*.cpp)
SRCS = $(filter-out $(SRC_DIR)/main_v%, $(ALL_SRCS))

# Crea la lista dei file oggetto (sostituisce src/file.cpp con build/objects/file.o)
OBJS = $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Regola di default
all: $(TARGET)

# Regola per creare l'eseguibile finale
$(TARGET): $(OBJS)
	@mkdir -p build
	$(CXX) $(OBJS) -o $(TARGET)
	@echo "Build completata: $(TARGET)"

# Regola per compilare i file sorgente in oggetti
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Pulizia dei file compilati
clean:
	rm -rf build


# Uso: make release V=1.0
release: all
	@if [ -z "$(V)" ]; then \
		echo "Errore: Devi specificare una versione. Usa 'make release V=1.0'"; \
		exit 1; \
	fi
	@echo "Creazione dello snapshot per la versione v$(V)..."
	@mkdir -p gauntlet/versions/v$(V)/src
	@mkdir -p gauntlet/versions/v$(V)/include
	@cp -r src/* gauntlet/versions/v$(V)/src/
	@cp -r include/* gauntlet/versions/v$(V)/include/
	@echo "Compressione dei sorgenti in v$(V).tar.gz in corso..."
	@tar -czf gauntlet/versions/v$(V).tar.gz -C gauntlet/versions v$(V)
	@echo "Pulizia della cartella temporanea..."
	@rm -rf gauntlet/versions/v$(V)
	@echo "Salvataggio dell'eseguibile..."
	@cp build/main gauntlet/versions/engine_v$(V)
	@echo "Versione v$(V) rilasciata con successo!"
	@echo "Archivio sorgenti: gauntlet/versions/v$(V).tar.gz"
	@echo "Eseguibile pronto: gauntlet/versions/engine_v$(V)"

# Evita conflitti con file chiamati 'all' o 'clean'
.PHONY: all clean release

