# Nome dell'eseguibile
TARGET = build/main

# Compilatore e flag
CXX = g++ -D_DEBUG
CXXFLAGS = -Wall -Wextra -std=c++17 -Iinclude

# Cartelle
SRC_DIR = src
OBJ_DIR = build/objects

# Trova tutti i file .cpp nella cartella src
SRCS = $(wildcard $(SRC_DIR)/*.cpp)

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

# Evita conflitti con file chiamati 'all' o 'clean'
.PHONY: all clean