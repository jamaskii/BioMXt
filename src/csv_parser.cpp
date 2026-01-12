#include "biomxt/csv_parser.hpp"
#include <charconv>

namespace biomxt {

    std::vector<std::string_view> parse_line(const std::string& line, uint64_t& reserve_size, char separation) {
        std::vector<std::string_view> cells;
        if (line.empty()) return cells;

        cells.reserve(reserve_size == 0 ? 100 : reserve_size);

        size_t start = 0;
        bool in_quote = false;
        size_t n = line.size();

        for (size_t i = 0; i < n; ++i) {
            if (line[i] == '"') {
                in_quote = !in_quote; // 切换引号状态
            } else if (line[i] == separation && !in_quote) {
                // 找到分隔符，直接截取原始内存区间
                cells.emplace_back(line.data() + start, i - start);
                start = i + 1;
            }
        }
        // 放入最后一个单元格
        cells.emplace_back(line.data() + start, n - start);

        if (cells.size() > reserve_size) reserve_size = cells.size();
        return cells;
    }

} // namespace biomxt