# 1. 配置参数
CXX      = g++
CXXFLAGS = -O3 -std=c++17 -Iinclude

# 简单的系统检测
ifeq ($(OS),Windows_NT)
    # Windows 环境 (MinGW)
    RM = if exist $(1) rmdir /s /q $(1)
    EXE_EXT = .exe
    MKDIR = if not exist $(1) mkdir $(1)
    # Windows 下通常需要手动指定 Zstd 路径
    CXXFLAGS += -Ithird_party/zstd/include
    LDFLAGS  = -Lthird_party/zstd/lib -lzstd
else
    # Linux/WSL 环境
    RM = rm -rf $(1)
    EXE_EXT =
    MKDIR = mkdir -p $(1)
    # Linux 通常通过 apt 安装 libzstd-dev，系统会自动找路径
    LDFLAGS  = -lzstd
endif

# 2. 自动遍历
SRCS     = $(wildcard src/*.cpp)
OBJS     = $(patsubst src/%.cpp, build/%.o, $(SRCS))

CLI_SRC  = apps/cli.cpp
TEST_SRC = tests/test.cpp

# 3. 目标定义 (加入后缀变量)
LIB_TARGET  = lib/libbiomxt.a
CLI_TARGET  = bin/biomxt$(EXE_EXT)
TEST_TARGET = bin/test$(EXE_EXT)

# 4. 任务规则
all: $(LIB_TARGET) $(CLI_TARGET) test

# 使用 $(MKDIR) 变量，确保跨平台兼容
build/%.o: src/%.cpp
	@$(call MKDIR, build)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(LIB_TARGET): $(OBJS)
	@$(call MKDIR, lib)
	ar rcs $@ $(OBJS)

cli: $(LIB_TARGET)
	@$(call MKDIR, bin)
	$(CXX) $(CXXFLAGS) $(CLI_SRC) $(LIB_TARGET) -o $(CLI_TARGET) $(LDFLAGS)

test: $(LIB_TARGET)
	@$(call MKDIR, bin)
	$(CXX) $(CXXFLAGS) $(TEST_SRC) $(LIB_TARGET) -o $(TEST_TARGET) $(LDFLAGS)
	@echo --- Running Tests ---
	@./$(TEST_TARGET)

clean:
	@$(call RM, build)
	@$(call RM, bin)
	@$(call RM, lib)