PROJECT := aria

SRC_DIR := src
INC_DIR := $(SRC_DIR)/include

BUILD_DIR := build
BIN_DIR := $(BUILD_DIR)/bin
OBJ_DIR := $(BUILD_DIR)/obj

DEPS_DIR := deps
DOCS_DIR := docs

C_FILES := $(shell find $(SRC_DIR) -name "*.c")
ASM_FILES := $(shell find $(SRC_DIR) -name "*.asm")

OBJ_FILES := $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(C_FILES)))
OBJ_FILES += $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(ASM_FILES)))
BIN_FILE := $(BIN_DIR)/$(PROJECT)

CC := gcc
LD := gcc

PREPROCESSOR_DEFINES := -DTAB_COUNT=4 -DAST_TAB_COUNT=2
CFLAGS := $(PREPROCESSOR_DEFINES) -I$(INC_DIR) -std=c99 -pedantic -Wall -Wextra -Wunused -Wshadow -Wno-write-strings -Wdouble-promotion -Wduplicate-decl-specifier -Wformat=2 -Winit-self -Wmisleading-indentation -Wswitch-default -Wstrict-overflow -Walloca -Wconversion -Wunused-macros -Wdate-time -Waddress -Wlogical-op -Wlogical-not-parentheses -Wstrict-prototypes -Wpacked -Winline -m64 -g
ASMFLAGS := -felf64
LDFLAGS :=

LIBS_INC_DIR_CMD :=
LIBS_LIB_DIR_CMD :=
LIBS_LIB_CMD :=

CMD_ARGS := examples/expr_test.ar

run: clean $(BIN_FILE) docs
	$(BIN_FILE) $(CMD_ARGS)

debug: clean $(BIN_FILE)
	gdb --args $(BIN_FILE) $(CMD_ARGS)

$(BIN_FILE): $(OBJ_FILES)
	@mkdir -p $(dir $@)
	$(LD) -o $@ $(OBJ_FILES) $(LIBS_LIB_DIR_CMD) $(LIBS_LIB_CMD)

$(OBJ_DIR)/%.c.o: %.c
	@mkdir -p $(OBJ_DIR)/$(dir $^)
	$(CC) -c $(CFLAGS) $(LIBS_INC_DIR_CMD) -o $@ $^

$(OBJ_DIR)/%.asm.o: %.asm
	@mkdir -p $(OBJ_DIR)/$(dir $^)
	nasm $(ASMFLAGS) -o $@ $^

clean:
	rm -rf $(BUILD_DIR)
	cd $(DOCS_DIR) && $(MAKE) clean

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
