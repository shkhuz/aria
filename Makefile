CXX_FILES := src/main.cpp vendor/fmt/format.cc 
OBJ_FILES := $(addprefix build/obj/, $(addsuffix .o, $(CXX_FILES)))
EXE_FILE := build/ariac
ALL_CXX_FILES := $(shell find src -type f -name "*.cpp") #$(shell find src -type f -name "*.cpp" | grep -v $(NO_CXX_FILES))

CXXFLAGS := -std=c++11 -Ivendor -I. -Wall -Wextra -Wshadow -Wno-switch -Wno-unused-function -Wno-unused-parameter -Wno-write-strings
LDFLAGS := -L/usr/lib -lLLVM
AR_FILE := examples/ok.ar std/std.ar

ifeq ($(prod), y)
	CXXFLAGS_OPTIMIZE := -O3
else
	CXXFLAGS_OPTIMIZE := -g -O0
endif

CXX := g++
LD := g++

run: $(EXE_FILE)
	./$^ -o build/app $(AR_FILE)

$(EXE_FILE): $(OBJ_FILES)
	@mkdir -p $(dir $@)
	$(LD) -o $@ $(LDFLAGS) $(OBJ_FILES)

build/obj/src/main.cpp.o: $(ALL_CXX_FILES)
	@mkdir -p $(dir $@)
	$(CXX) -c $(CXXFLAGS) $(CXXFLAGS_OPTIMIZE) -o $@ src/main.cpp

build/obj/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) -c $(CXXFLAGS) $(CXXFLAGS_OPTIMIZE) -o $@ $^

build/obj/%.cc.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) -c $(CXXFLAGS) $(CXXFLAGS_OPTIMIZE) -o $@ $^

debug: $(EXE_FILE)
	gdb --args $^ $(AR_FILE)

clean:
	rm -rf build/ a.out *.o

