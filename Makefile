# 1. 配置参数
CXX      = g++
CXXFLAGS = -O3 -std=c++17 -Iinclude -Ithird_party/zstd/include
# LDFLAGS  = -Lthird_party/zstd/lib -lzstd

# 2. 自动遍历 (Magic Parts)
# 自动寻找 src 目录下所有的 .cpp 文件
SRCS     = $(wildcard src/*.cpp)
# 将 SRCS 中所有的 .cpp 后缀替换为 build 目录下的 .o 后缀
OBJS     = $(patsubst src/%.cpp, build/%.o, $(SRCS))

CLI_SRC = apps/cli.cpp
TEST_SRC = tests/test.cpp

# 3. 目标定义
LIB_TARGET  = lib/libbiomxt.a
CLI_TARGET  = bin/biomxt.exe
TEST_TARGET = bin/test.exe

# 4. 任务规则
all: $(LIB_TARGET) $(CLI_TARGET) test_run

# 自动编译：任何 build/*.o 都依赖对应的 src/*.cpp
build/%.o: src/%.cpp
	@if not exist build mkdir build
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 打包静态库：依赖所有的 OBJS
$(LIB_TARGET): $(OBJS)
	@if not exist lib mkdir lib
	ar rcs $@ $(OBJS)

# 编译 CLI 工具
cli: $(LIB_TARGET)
	@if not exist bin mkdir bin
	$(CXX) $(CXXFLAGS) $(CLI_SRC) $(LIB_TARGET) -o $(CLI_TARGET) $(LDFLAGS)

# 编译并运行测试
test: $(LIB_TARGET)
	@if not exist bin mkdir bin
	$(CXX) $(CXXFLAGS) $(TEST_SRC) $(LIB_TARGET) -o $(TEST_TARGET) $(LDFLAGS)
	@echo --- Running Tests ---
	@$(TEST_TARGET)

clean:
	if exist build rmdir /s /q build
	if exist bin rmdir /s /q bin
	if exist lib rmdir /s /q lib