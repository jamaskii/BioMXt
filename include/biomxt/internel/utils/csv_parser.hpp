#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <charconv>
#include <cmath>
#include <stdexcept>
#include <fstream>
#include "zstd.h"
#include "zstd_errors.h"
#include "biomxt/internel/struct/data_type.hpp"


namespace biomxt {

    /**
     * @brief Parse a csv line, storage cells into provided vector, return count of cells obtained.
     * @param line A csv line to be parsed.
     * @param cells A vector to store parsed cells.
     * @param separator A char to be used as separator during parsing.
     * @return uint32_t Count of cells obtained.
     * @throws std::invalid_argument If line contains unclosed quote.
     * @throws std::out_of_range If line contains too much cells, exceeds cells vector size.
     */
    uint32_t csv_parse_line(
        const std::string& line, 
        std::vector<std::string>& cells,
        const char separator);

    /**
     * @brief Parse a csv line, only return count of cells obtained.
     * @param line A csv line to be parsed.
     * @param separator A char to be used as separator during parsing.
     * @return uint32_t Count of cells obtained.
     * @throws std::invalid_argument If line contains unclosed quote.
     */
    uint32_t csv_parse_line(
        const std::string& line, 
        const char separator);

    /**
     * @brief Convert string to specified type.
     * @param str String to be converted.
     * @return T Converted value.
     * @throws std::out_of_range If value exceeds range of T.
     */
    template <typename T> inline T string_to(const std::string& str) {
        static_assert(dtype_from_type<T>::valid, "biomxt::string_to<T>: Unsupported type. Only int16_t, int32_t, int64_t, float, and double are allowed.");
        return T{}; 
    }
    template <> inline int16_t string_to<int16_t>(const std::string& str) {
        int32_t value = std::stoi(str);
        if (value < INT16_MIN || value > INT16_MAX) {
            throw std::out_of_range("biomxt::string_to<int16_t>: Value out of range for int16_t.");
        }
        return static_cast<int16_t>(value);
    }
    template <> inline int32_t string_to<int32_t>(const std::string& str) { return static_cast<int32_t>(std::stoi(str)); }
    template <> inline int64_t string_to<int64_t>(const std::string& str) { return std::stoll(str); }
    template <> inline float string_to<float>(const std::string& str) { 
        float val;
        auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), val);
        if (ec != std::errc()) {
            throw std::invalid_argument("biomxt::string_to<float>: Failed to parse float value.");
        }
        return val;
    }
    template <> inline double string_to<double>(const std::string& str) { return std::stod(str); }

}