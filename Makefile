PROJECT := aria

SRC_DIR := src
BUILD_DIR := build
BIN_DIR := $(BUILD_DIR)/bin
OBJ_DIR := $(BUILD_DIR)/obj
DEPS_DIR := deps
DOCS_DIR := docs

C_FILES := src/main.c
ASM_FILES := $(shell find $(SRC_DIR) -name "*.asm")
OBJ_FILES := $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(C_FILES)))
OBJ_FILES += $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(ASM_FILES)))
BIN_FILE := $(BIN_DIR)/$(PROJECT)

CC := gcc
LD := gcc

PREPROCESSOR_DEFINES := -DTAB_COUNT=4 -DAST_TAB_COUNT=4
ifeq ($(warn), yes)
	WARN_CFLAGS := 
else
	WARN_CFLAGS := -Wno-switch -Wno-unused-variable -Wno-unused-parameter
endif
CFLAGS := $(PREPROCESSOR_DEFINES) -I$(SRC_DIR) -I. -Wall -Wextra -Wshadow $(WARN_CFLAGS) -std=gnu99 -m64 -g -O0
ASMFLAGS := -felf64
LDFLAGS :=
LIBS_INC_DIR_CMD :=
LIBS_LIB_DIR_CMD :=
LIBS_LIB_CMD :=
#CMD_ARGS := examples/test.ar examples/single.ar examples/usage.ar
#CMD_ARGS := examples/single.ar 
CMD_ARGS := examples/pub_test.ar 

all: $(BIN_FILE) #docs
	$(BIN_FILE) $(CMD_ARGS)

debug: $(BIN_FILE)
	gdb --args $(BIN_FILE) $(CMD_ARGS)

$(BIN_FILE): clean $(OBJ_FILES)
	@mkdir -p $(dir $@)
	$(LD) -o $@ $(OBJ_FILES) $(LIBS_LIB_DIR_CMD) $(LIBS_LIB_CMD) $(LDFLAGS)

$(OBJ_DIR)/%.c.o: %.c
	@mkdir -p $(OBJ_DIR)/$(dir $^)
	$(CC) -c $(CFLAGS) $(LIBS_INC_DIR_CMD) -o $@ $^

$(OBJ_DIR)/%.asm.o: %.asm
	@mkdir -p $(OBJ_DIR)/$(dir $^)
	nasm $(ASMFLAGS) -o $@ $^

clean:
	rm -f $(OBJ_FILES)
	rm -rf $(OBJ_DIR)
	rm -f $(BIN_FILE)
	rm -fd $(BIN_DIR)
	rm -fd $(BUILD_DIR)
	rm -f a.out
	#cd $(DOCS_DIR) && $(MAKE) clean

docs:
	cd $(DOCS_DIR) && $(MAKE)

loc:
	find $(SRC_DIR) \
		-name "*.cpp" -or \
		-name "*.hpp" -or \
		-name "*.h" -or \
		-name "*.c" -or \
		-name "*.asm" \
	| xargs cat | wc -l

.PHONY: clean loc docs
