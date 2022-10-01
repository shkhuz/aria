ALL_C_FILES := $(shell find src -type f -name "*.c") #$(shell find src -type f -name "*.cpp" | grep -v $(NO_CXX_FILES))
C_FILES := $(ALL_C_FILES)
OBJ_FILES := $(addprefix build/obj/, $(addsuffix .o, $(C_FILES)))
EXE_FILE := build/ariac

CFLAGS := -std=c99 -Ivendor -I. `llvm-config --cflags` -Wall -Wextra -Wshadow -Wno-switch -Wno-unused-function -Wno-unused-parameter -Wno-write-strings
LDFLAGS := `llvm-config --ldflags --libs`
AR_FILE := examples/arrays.ar

ifeq ($(prod), y)
	CFLAGS_OPTIMIZE := -O3
else
	CFLAGS_OPTIMIZE := -g -O0
endif

CC := gcc
LD := gcc

run: $(EXE_FILE)
	./$^ -o build/app $(AR_FILE)

$(EXE_FILE): $(OBJ_FILES)
	@mkdir -p $(dir $@)
	$(LD) -o $@ $(LDFLAGS) $(OBJ_FILES)

# build/obj/src/main.c.o: $(ALL_C_FILES)
# 	@mkdir -p $(dir $@)
# 	$(C) -c $(CFLAGS) $(CFLAGS_OPTIMIZE) -o $@ src/main.c

build/obj/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(CFLAGS_OPTIMIZE) -o $@ $^

build/obj/%.cc.o: %.cc
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(CFLAGS_OPTIMIZE) -o $@ $^

debug: $(EXE_FILE)
	gdb --args $^ $(AR_FILE)

install: $(EXE_FILE)
	install -Dm557 $^ "$(DESTDIR)/usr/bin/ariac"

clean:
	rm -rf build/ a.out *.o

.PHONY: run debug install clean
