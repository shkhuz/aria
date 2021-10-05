C_FILES := $(shell find src -type f -name "*.c")
H_FILES := $(shell find src -type f -name "*.h")
OBJ_FILES := $(addprefix build/obj/, $(addsuffix .o, $(C_FILES)))
EXE_FILE := build/ariac
CFLAGS := -Wall -Wextra -Wshadow -Wno-switch -Wno-unused-function -Wno-unused-parameter
LDFLAGS := 
AR_FILE := examples/cmp_test.ar

ifeq ($(prod), y)
	CFLAGS_OPTIMIZE := -O3
else
	CFLAGS_OPTIMIZE := -g -O0
endif

CC := gcc
LD := gcc

run: $(EXE_FILE)
	./$^ -o build/app examples/std.ar $(AR_FILE)
	./build/app

$(EXE_FILE): $(OBJ_FILES)
	@mkdir -p $(dir $@)
	$(LD) -o $@ $(LDFLAGS) $(OBJ_FILES)

build/obj/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(CFLAGS_OPTIMIZE) -o $@ $^

debug: $(EXE_FILE)
	gdb --args $^ $(AR_FILE)

clean:
	rm -rf build/ a.out *.o *.asm

