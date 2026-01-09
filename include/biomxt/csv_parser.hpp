#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdint>

namespace biomxt {

// 字符串处理工具
void ltrim(std::string &s);
void rtrim(std::string &s);
void trim(std::string &s);

// CSV 解析
std::vector<std::string> parse_line(const std::string& line, uint64_t& reserve_size, char separation = ',');

} // namespace biomxt