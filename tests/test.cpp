#include <iostream>
#include <cassert>
#include "biomxt/csv_parser.hpp"

void run_test() {
    // 模拟一行复杂的 CSV 数据
    // 原始字符串: "Gene A", 1.23, "Cell ""Alpha""", , -0.5e-10\r
    std::string test_line = "\"Gene A\",1.23,\"Cell \"\"Alpha\"\"\", ,-0.5e-10";

    size_t max_cells = 0;
    // 执行解析
    std::vector<std::string> result = biomxt::parse_line(test_line, max_cells);

    // 验证结果
    std::cout << "--- BioMxt CSV Parser Test ---" << std::endl;
    std::cout << "Line: " << test_line << std::endl;
    std::cout << "Parsed Cells Count: " << result.size() << std::endl;

    // 断言检查
    assert(result.size() == 5);           // 应该有 5 个单元格
    assert(result[0] == "Gene A");        // 自动去除了外层双引号
    assert(result[1] == "1.23");         // 逗号后的空格保留（后续可用 trim 处理）
    assert(result[2] == "Cell \"Alpha\""); // 处理了转义的双引号 "" -> "
    assert(result[3] == " ");             // 空格单元格（逗号之间只有空格）
    assert(result[4] == "-0.5e-10");     // 科学计数法字符串正常保留，且忽略了末尾 \r

    std::cout << "Status: ALL TESTS PASSED!" << std::endl;
    
    // 打印具体内容方便肉眼观察
    for(size_t i=0; i<result.size(); ++i) {
        std::cout << "Cell [" << i << "]: [" << result[i] << "]" << std::endl;
    }
}

int main() {
    try {
        run_test();
    } catch (const std::exception& e) {
        std::cerr << "Test failed with error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}