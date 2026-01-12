#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <charconv>
#include <cmath>


namespace biomxt {

    // CSV 解析
    std::vector<std::string_view> parse_line(const std::string& line, uint64_t& reserve_size, char separation = ',');

    inline float fast_atof(std::string_view sv) {
        if (sv.empty()) return 0.0f;

        const char* p = sv.data();
        const char* end = p + sv.size(); // 必须使用结束指针

        // 预计算 10 的幂次方表，涵盖 float 的常用范围
        static const float pow10[] = {
            1e0f, 1e1f, 1e2f, 1e3f, 1e4f, 1e5f, 1e6f, 1e7f, 1e8f, 1e9f, 1e10f
        };

        float res = 0.0f;
        bool neg = false;
        if (p < end && *p == '-') { neg = true; p++; }
        else if (p < end && *p == '+') { p++; }

        // 1. 解析整数部分
        while (p < end && *p >= '0' && *p <= '9') {
            res = res * 10.0f + (*p - '0');
            p++;
        }

        // 2. 解析小数部分
        if (p < end && *p == '.') {
            p++;
            float frac_part = 0.0f;
            float divisor = 1.0f;
            while (p < end && *p >= '0' && *p <= '9') {
                frac_part = frac_part * 10.0f + (*p - '0');
                divisor *= 10.0f;
                p++;
            }
            res += frac_part / divisor; // 仅除一次，保证精度
        }

        // 3. 解析科学计数法
        if (p < end && (*p == 'e' || *p == 'E')) {
            p++;
            bool exp_neg = false;
            if (p < end && *p == '-') { exp_neg = true; p++; }
            else if (p < end && *p == '+') { p++; }

            int exp_val = 0;
            while (p < end && *p >= '0' && *p <= '9') {
                exp_val = exp_val * 10 + (*p - '0');
                p++;
            }

            // 使用 std::pow 或查找表
            float factor = std::pow(10.0f, (float)exp_val);
            if (exp_neg) res /= factor;
            else res *= factor;
        }

        return neg ? -res : res;
    }

} // namespace biomxt