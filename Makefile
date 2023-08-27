ALL_COMPILER_C_FILES := $(shell find src -type f -name "*.c" -not -path "src/tests/*")
COMPILER_C_FILES := $(ALL_COMPILER_C_FILES)
COMPILER_OBJ_FILES := $(addprefix build/obj/, $(addsuffix .o, $(COMPILER_C_FILES)))

ALL_TEST_C_FILES := $(shell find src -type f -name "*.c" -not -path "src/main.c")
TEST_C_FILES := $(ALL_TEST_C_FILES)
TEST_OBJ_FILES := $(addprefix build/obj/, $(addsuffix .o, $(TEST_C_FILES)))

CFLAGS := -std=c99 -Ivendor -I. `llvm-config --cflags` -Wall -Wextra -Wshadow -Wno-switch -Wno-unused-function -Wno-unused-parameter -Wno-write-strings -Wno-switch-bool -Wno-varargs
LDFLAGS := `llvm-config --ldflags --libs`

PREFIX := /usr/local
EXE_NAME := aria
EXE_PATH := build/$(EXE_NAME)
LIB_NAME := aria
LIB_PATH := lib/$(LIB_NAME)

AR_FILE := examples/small2.ar

ifeq ($(prod), y)
	CFLAGS_OPTIMIZE := -O3
else
	CFLAGS_OPTIMIZE := -g -O0
endif

ifeq ($(v), y)
	TEST_CFLAGS := -DTEST_PRINT_COMPILER_MSGS
else
	TEST_CFLAGS :=
endif

CC := clang
LD := clang

run: $(EXE_PATH)
	./$^ -o build/app $(AR_FILE)

$(EXE_PATH): $(COMPILER_OBJ_FILES)
	@mkdir -p $(dir $@)
	$(LD) -o $@ $(LDFLAGS) $^

test: build/test
	./$^

build/test: $(TEST_OBJ_FILES)
	@mkdir -p $(dir $@)
	$(LD) -o $@ $(LDFLAGS) $^

# build/obj/src/main.c.o: $(ALL_C_FILES)
#	@mkdir -p $(dir $@)
#	$(C) -c $(CFLAGS) $(CFLAGS_OPTIMIZE) -o $@ src/main.c

build/obj/src/tests/tests.c.o: src/tests/tests.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(CFLAGS_OPTIMIZE) $(TEST_CFLAGS) -o $@ $^

build/obj/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(CFLAGS_OPTIMIZE) -o $@ $^

build/obj/%.cc.o: %.cc
	@mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $(CFLAGS_OPTIMIZE) -o $@ $^

debug: $(EXE_PATH)
	gdb -ex '$(DBG_ARGS)' -args $^ $(AR_FILE)

install: $(EXE_PATH)
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@mkdir -p $(DESTDIR)$(PREFIX)/lib
	cp -f $^ $(DESTDIR)$(PREFIX)/bin/$(EXE_NAME)
	chmod 755 $(DESTDIR)$(PREFIX)/bin/$(EXE_NAME)
	cp -rf $(LIB_PATH) $(DESTDIR)$(PREFIX)/lib

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(EXE_NAME)
	rm -rf $(DESTDIR)$(PREFIX)/lib/$(LIB_NAME)

clean:
	rm -rf build/ a.out *.o

.PHONY: run debug install uninstall clean
