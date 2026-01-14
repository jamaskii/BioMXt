#include "biomxt/csv_parser.hpp"
#include <charconv>

namespace biomxt {

    std::vector<std::string> parse_line(const std::string& line, uint64_t& reserve_size, const char separation) {
        std::vector<std::string> cells;
        std::string cell;
        
        // 性能优化, 预分配内存
        cells.reserve(reserve_size==0 ? 10 : reserve_size);

        // 1. 先确定真正的逻辑结尾，排除掉 \r 或 \n
        size_t end_pos = line.size();
        while (end_pos > 0 && (line[end_pos - 1] == '\r' || line[end_pos - 1] == '\n')) {
            end_pos--;
        }

        // 状态: 当前是否处于双引号内
        bool in_quote = false;

        for (size_t i = 0; i < end_pos; ++i) {
            
            // 检测双引号转义：当前字符是"，且下一个字符也是"
            if (line[i] == '"') {
                if (!in_quote){
                    // 进入双引号标志
                    in_quote = true;
                } else if ( i + 1 < end_pos && line[i+1] == '"'){
                    // 双引号转义, 直接追加
                    cell += '"';
                    // 跳过下一个转义用的双引号（避免重复处理）
                    ++i;
                } else {
                    in_quote = false;
                }
            } else if (line[i] == ',' && !in_quote) {
                // 逗号分隔符, 且不在双引号内
                cells.push_back(std::move(cell));
                cell.clear();
            } else {
                // 普通字符，直接追加
                cell += line[i];
            }
        }
        
        // 最后一个单元格不会有分隔符, 所以在这里接住
        cells.push_back(std::move(cell));

        // 记录最大单元格数
        if (cells.size() > reserve_size) {
            reserve_size = cells.size();
        }
        
        return cells;
    }

} // namespace biomxt