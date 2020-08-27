PROJECT := aria

SRC_DIR := src
INC_DIR := $(SRC_DIR)/include

BUILD_DIR := build
BIN_DIR := $(BUILD_DIR)/bin

all:
	mkdir -p $(BIN_DIR)
	gcc -o $(BIN_DIR)/aria -I $(INC_DIR) $(SRC_DIR)/aria.c
	$(BIN_DIR)/aria

clean:
	rm -rf $(BUILD_DIR)

.PHONY: clean
