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

    inline float fast_atof(const char* p) {
        if (!p || !*p) return 0.0f;

        float res = 0.0f;
        bool neg = false;
        if (*p == '-') { neg = true; p++; }
        else if (*p == '+') { p++; }

        // 1. 解析整数部分
        while (*p >= '0' && *p <= '9') {
            res = res * 10.0f + (*p - '0');
            p++;
        }

        // 2. 解析小数部分
        if (*p == '.') {
            p++;
            float frac = 0.1f;
            while (*p >= '0' && *p <= '9') {
                res += (*p - '0') * frac;
                frac /= 10.0f;
                p++;
            }
        }

        // 3. 解析科学计数法 (e 或 E)
        if (*p == 'e' || *p == 'E') {
            p++;
            bool exp_neg = false;
            if (*p == '-') { exp_neg = true; p++; }
            else if (*p == '+') { p++; }

            int exp_val = 0;
            while (*p >= '0' && *p <= '9') {
                exp_val = exp_val * 10 + (*p - '0');
                p++;
            }

            // 使用预计算或简单的 pow 处理指数
            float factor = 1.0f;
            float base = 10.0f;
            int e = exp_val;
            while (e > 0) {
                if (e & 1) factor *= base;
                base *= base;
                e >>= 1;
            }

            if (exp_neg) res /= factor;
            else res *= factor;
        }

        return neg ? -res : res;
    }

} // namespace biomxt