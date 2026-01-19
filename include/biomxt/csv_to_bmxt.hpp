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
#include "biomxt/spec.hpp"
#include "zstd.h"
#include "zstd_errors.h"


namespace biomxt {

    /**
     * @brief Parse a csv line, storage cells into provided vector, return count of cells obtained.
     * @param line A csv line to be parsed.
     * @param cells A vector to store parsed cells.
     * @param separator A char to be used as separator during parsing.
     * @return uint64_t Count of cells obtained.
     * @throws std::invalid_argument If line contains unclosed quote.
     * @throws std::out_of_range If line contains too much cells, exceeds cells vector size.
     */
    uint64_t csv_parse_line(
        const std::string& line, 
        std::vector<std::string>& cells,
        const char separator);

    /**
     * @brief Parse a csv line, only return count of cells obtained.
     * @param line A csv line to be parsed.
     * @param separator A char to be used as separator during parsing.
     * @return uint64_t Count of cells obtained.
     * @throws std::invalid_argument If line contains unclosed quote.
     */
    uint64_t csv_parse_line(
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
    template <> inline float string_to<float>(const std::string& str) { return std::stof(str); }
    template <> inline double string_to<double>(const std::string& str) { return std::stod(str); }

    /**
     * @brief Flush rows buffer by spliting it into blocks, then compress each block and write to file, store index entries in block table.
     * @param rows_buffer Rows buffer to be flushed.
     * @param block_width Width of each block.
     * @param actual_block_height Height of each block.
     * @param block_table Block table to store index entries.
     * @param out Output file stream.
     * @param block Block buffer to store compressed data.
     * @param compress_buffer Compress buffer to store compressed data.
     * @param algo Compression algorithm to be used.
     * @throws `std::invalid_argument` If rows_buffer is empty.
     * @throws `std::invalid_argument` If compress algo is not supported.
     * @throws `std::runtime_error` If compression fails.
     */
    template <typename T> void flush_rows_buffer(
        const std::vector<std::vector<T>>& rows_buffer, 
        uint32_t block_width, 
        uint32_t actual_block_height,
        std::vector<biomxt::IndexEntry>& block_table,
        std::ofstream& out,
        std::vector<T>& block,
        std::vector<char>& compress_buffer,
        biomxt::CompressAlgo algo);

    /**
     * @brief Convert a csv file to biomxt format.
     * @param input_file Path to input csv file.
     * @param output_file Path to output biomxt file.
     * @param block_width Width of each block.
     * @param block_height Height of each block.
     * @param separator Separator to be used for csv parsing, default is `,`.
     * @param algo Compression algorithm to be used.
     * @param warnings A vector to store warnings.
     * @return `biomxt::FileHeader` File header of output biomxt file.
     * @throws `std::invalid_argument` If block width or height is not greater than 0.
     * @throws `std::runtime_error` If conversion fails.
     */
    template <typename T>biomxt::FileHeader csv_to_bmxt(
        const std::string& input_file, 
        const std::string& output_file, 
        uint32_t block_width, 
        uint32_t block_height, 
        char separator, 
        biomxt::CompressAlgo algo,
        std::vector<std::string>& warnings);
}
