#include "biomxt/csv_parser.hpp"
#include <charconv>

namespace biomxt {

    // 左去除空白
    void ltrim(std::string &s) {
        // 找到第一个非空白字符的位置
        auto pos = std::find_if(s.begin(), s.end(), [](unsigned char c) {
            return !std::isspace(c);
        });
        // 从开头删除到该位置之前的所有字符（即删除所有开头空白）
        s.erase(s.begin(), pos);
    }

    // 右去除空白
    void rtrim(std::string &s) {
        // 找到最后一个非空白字符的位置
        auto pos = std::find_if(s.rbegin(), s.rend(), [](unsigned char c) {
            return !std::isspace(c);
        }).base();  // rbegin() 是反向迭代器，base() 转换为普通迭代器
        // 从该位置之后删除到字符串末尾（即删除所有末尾空白）
        s.erase(pos, s.end());
    }

    // 首尾去除空白
    void trim(std::string &s) {
        ltrim(s);
        rtrim(s);
    }

    std::vector<std::string> parse_line(const std::string& line, uint64_t& reserve_size, char separation) {
        std::vector<std::string> cells;
        std::string cell;

        if (line.empty()) return cells;
        
        // 性能优化, 预分配内存
        cells.reserve(reserve_size==0 ? 10 : reserve_size);

        uint64_t end_pos = line.size();

        // 状态: 当前是否处于双引号内
        bool in_quote = false;

        for (uint64_t i = 0; i < end_pos; ++i) {
            
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
            } else if (line[i] == separation && !in_quote) {
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