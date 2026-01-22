#### Compiler and flags ####
CXX      = g++
CXXFLAGS = -O3 -std=c++17 -Iinclude -MMD -MP

#### System type detection ####
ifeq ($(OS),Windows_NT)
    # Windows (MinGW)
    RM = if exist $(1) rmdir /s /q $(1)
    EXE_EXT = .exe
    MKDIR = mkdir $(subst /,\,$(1)) >nul 2>&1 || echo.
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
SRC_DIRS = src src/utils
SRCS = $(foreach dir, $(SRC_DIRS), $(wildcard $(dir)/*.cpp))
OBJS     = $(patsubst src/%.cpp, build/%.o, $(SRCS))

# Public headers to be exposed to users
PUBLIC_HEADERS = include/biomxt/biomxt_file.hpp include/biomxt/biomxt_converter.hpp include/biomxt/biomxt_types.hpp

# Library
LIB_TARGET  = lib/libbiomxt.a

# CLI application
CLI_SRC  = apps/cli.cpp
CLI_TARGET  = bin/biomxt$(EXE_EXT)

# Test targets
TEST_CSV_SRC = tests/test_csv.cpp
TEST_CSV_TARGET = bin/test_csv$(EXE_EXT)

TEST_ZSTD_SRC = tests/test_zstd.cpp
TEST_ZSTD_TARGET = bin/test_zstd$(EXE_EXT)

TEST_CONV_SRC = tests/test_conv.cpp
TEST_CONV_TARGET = bin/test_conv$(EXE_EXT)

TEST_CACHE_SRC = tests/test_cache.cpp
TEST_CACHE_TARGET = bin/test_cache$(EXE_EXT)

#### Task rules ####
.PHONY: all lib cli test clean install package

# Default target: build everything
all: lib cli

# Build the core library
lib: $(LIB_TARGET)

# Build the CLI application
cli: $(CLI_TARGET)

# Build all tests
test: test_csv test_zstd test_conv test_cache

# Compile object files - fixed to handle subdirectories properly
build/%.o: src/%.cpp
	@$(call MKDIR, $(dir $@))
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Create static library
$(LIB_TARGET): $(OBJS)
	@$(call MKDIR, lib)
	ar rcs $@ $(OBJS)

# Build CLI application
$(CLI_TARGET): $(LIB_TARGET)
	@$(call MKDIR, bin)
	$(CXX) $(CXXFLAGS) $(CLI_SRC) $(LIB_TARGET) -o $(CLI_TARGET) $(LDFLAGS)

# Test targets
test_csv: $(LIB_TARGET)
	@$(call MKDIR, bin)
	$(CXX) $(CXXFLAGS) $(TEST_CSV_SRC) $(LIB_TARGET) -o $(TEST_CSV_TARGET) $(LDFLAGS)
	@echo --- Running CSV Tests ---
	@./$(TEST_CSV_TARGET)

test_zstd:
	@$(call MKDIR, bin)
	$(CXX) $(CXXFLAGS) $(TEST_ZSTD_SRC) -o $(TEST_ZSTD_TARGET) $(LDFLAGS)
	@echo --- Running Zstd Performance Test ---
	@./$(TEST_ZSTD_TARGET)

test_conv: $(LIB_TARGET)
	@$(call MKDIR, bin)
	$(CXX) $(CXXFLAGS) $(TEST_CONV_SRC) $(LIB_TARGET) -o $(TEST_CONV_TARGET) $(LDFLAGS)
	@echo --- Running Conversion Tests ---
	@./$(TEST_CONV_TARGET)

test_cache: $(LIB_TARGET)
	@$(call MKDIR, bin)
	$(CXX) $(CXXFLAGS) $(TEST_CACHE_SRC) $(LIB_TARGET) -o $(TEST_CACHE_TARGET) $(LDFLAGS)
	@echo --- Running Cache Tests ---
	@./$(TEST_CACHE_TARGET)

# Install headers and library to system (for development)
install:
	@echo "Installing BioMXt headers and library..."
	@$(call MKDIR, /usr/local/include/biomxt)
	cp $(PUBLIC_HEADERS) /usr/local/include/biomxt/
	cp $(LIB_TARGET) /usr/local/lib/
	@echo "Installation complete."

# Create a distribution package for other developers
package: lib
	@echo "Creating BioMXt distribution package..."
	@$(call MKDIR, dist/biomxt/include/biomxt)
	@$(call MKDIR, dist/biomxt/lib)
	cp $(PUBLIC_HEADERS) dist/biomxt/include/biomxt/
	cp $(LIB_TARGET) dist/biomxt/lib/
	cp LICENSE README.md dist/biomxt/
	@echo "Package created in dist/biomxt/"

# Clean build artifacts
clean:
	@$(call RM, build)
	@$(call RM, bin)
	@$(call RM, lib)
	@$(call RM, dist)

# Include dependency files
-include $(OBJS:.o=.d)