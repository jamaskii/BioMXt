#### Compiler and flags ####
CXX      = g++
CXXFLAGS = -O3 -std=c++17 -Iinclude -MMD -MP

#### System type detection ####
ifeq ($(OS),Windows_NT)
    # Windows (MinGW)
    RM = if exist $(1) rmdir /s /q $(1)
    EXE_EXT = .exe
    MKDIR = if not exist $(1) mkdir $(1)
    # on Windows, Zstd was included by refer to static library in third_party/zstd
    CXXFLAGS += -Ithird_party/zstd/include
    LDFLAGS  = -Lthird_party/zstd/lib -lzstd
else
    # Linux
    RM = rm -rf $(1)
    EXE_EXT =
    MKDIR = mkdir -p $(1)
    # on Linux, Zstd was included by refer to libzstd-dev installed by package manager like apt
    LDFLAGS  = -lzstd
endif


#### Source code and object ####
SRCS     = $(wildcard src/*.cpp)
OBJS     = $(patsubst src/%.cpp, build/%.o, $(SRCS))

# Library
LIB_TARGET  = lib/libbiomxt.a

# CLI
CLI_SRC  = apps/cli.cpp
CLI_TARGET  = bin/biomxt$(EXE_EXT)

# csv test
TEST_CSV_SRC = tests/test_csv.cpp
TEST_CSV_TARGET = bin/test$(EXE_EXT)

# zstd test
TEST_ZSTD_SRC = tests/test_zstd.cpp
TEST_ZSTD_TARGET = bin/test_zstd$(EXE_EXT)

# conv test
TEST_CONV_SRC = tests/test_conv.cpp
TEST_CONV_TARGET = bin/test_conv$(EXE_EXT)

#### Task rules ####
.PHONY: all cli test_csv test_zstd test_conv clean
all: $(LIB_TARGET) $(CLI_TARGET)

# Using $(MKDIR) variable to ensure cross-platform compatibility
build/%.o: src/%.cpp
	@$(call MKDIR, build)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(LIB_TARGET): $(OBJS)
	@$(call MKDIR, lib)
	ar rcs $@ $(OBJS)

cli: $(LIB_TARGET)
	@$(call MKDIR, bin)
	$(CXX) $(CXXFLAGS) $(CLI_SRC) $(LIB_TARGET) -o $(CLI_TARGET) $(LDFLAGS)

test_csv: $(LIB_TARGET)
	@$(call MKDIR, bin)
	$(CXX) $(CXXFLAGS) $(TEST_CSV_SRC) $(LIB_TARGET) -o $(TEST_CSV_TARGET) $(LDFLAGS)
	@echo --- Running Tests ---
	@./$(TEST_CSV_TARGET)

test_zstd:
	@$(call MKDIR, bin)
	$(CXX) $(CXXFLAGS) $(TEST_ZSTD_SRC) -o $(TEST_ZSTD_TARGET) $(LDFLAGS)
	@echo --- Running Zstd Performance Test ---
	@./$(TEST_ZSTD_TARGET)

test_conv: $(LIB_TARGET)
	@$(call MKDIR, bin)
	$(CXX) $(CXXFLAGS) $(TEST_CONV_SRC) $(LIB_TARGET) -o $(TEST_CONV_TARGET) $(LDFLAGS)
	@echo --- Running Conv Tests ---
	@./$(TEST_CONV_TARGET)

clean:
	@$(call RM, build)
	@$(call RM, bin)
	@$(call RM, lib)