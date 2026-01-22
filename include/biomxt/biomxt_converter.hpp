#pragma once
#include <charconv>
#include <vector>
#include <fstream>
#include "zstd.h"
#include "zstd_errors.h"
#include "biomxt/utils/csv_parser.hpp"
#include "biomxt/struct/index_entry.hpp"
#include "biomxt/struct/compress_algorithm.hpp"
#include "biomxt/struct/file_header.hpp"


namespace biomxt
{
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
        biomxt::CompressAlgorithm algo);

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
        biomxt::CompressAlgorithm algo,
        std::vector<std::string>& warnings);

} // namespace biomxt
