C_FILES := $(shell find src -type f -name "*.c")
H_FILES := $(shell find src -type f -name "*.h")
OBJ_FILES := $(addprefix obj/, $(addsuffix .o, $(C_FILES)))
EXE_FILE := bin/ariac
CFLAGS := -Wall -Wextra -Wshadow -Wno-switch -Wno-unused-function -Wno-unused-parameter
#CMD_ARGS := examples/cg.ar
CMD_ARGS := examples/lex.ar

ifeq ($(prod), y)
	CFLAGS_OPTIMIZE := -O3
else
	CFLAGS_OPTIMIZE := -g -O0
endif

CC := gcc
LD := gcc

run: $(EXE_FILE)
	./$^ $(CMD_ARGS)

$(EXE_FILE): $(OBJ_FILES)
	@mkdir -p $(dir $@)
	$(LD) -o $@ $(OBJ_FILES)

obj/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS_OPTIMIZE) -o $@ $(CFLAGS) $^

debug: $(EXE_FILE)
	gdb --args $^ $(CMD_ARGS)

clean:
	rm -rf obj/ bin/

