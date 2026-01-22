#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include "zstd.h"
#include <functional>
#include <typeinfo>
#include <algorithm>
#include <list>
#include <memory>
#include "./cache/block_cache.hpp"
#include "./struct/cells.hpp"
#include "./struct/file_header.hpp"
#include "./struct/compress_algorithm.hpp"
#include "./struct/uuid.hpp"
#include "./struct/data_type.hpp"
#include "./struct/index_entry.hpp"


namespace biomxt {
    class BlockCache;
    struct FileHeader;
    
    class BiomxtFile {
        public:
            /**
             * @brief                           Constructor, with outer cache.
             * 
             * @param path                      The path to the Biomxt file to open.
             * @param cache_entries             The cache entries to use. If nullptr, use the internal cache_entries.
             * @throws `std::runtime_error`     If the file cannot be opened.
             * @throws `std::runtime_error`     If the file has a bad header.
             * @throws `std::runtime_error`     If the file has a bad magic.
             * @throws `std::runtime_error`     If the file has a bad block table offset.
             * @throws `std::runtime_error`     If the file has a bad name table offset.
             */
            BiomxtFile(const std::string& path, BlockCache* block_cache);

            /**
             * @brief                           Constructor, without outer cache.
             * 
             * @param path                      The path to the Biomxt file to open.
             * @throws `std::runtime_error`     If the file cannot be opened.
             * @throws `std::runtime_error`     If the file has a bad header.
             * @throws `std::runtime_error`     If the file has a bad magic.
             * @throws `std::runtime_error`     If the file has a bad block table offset.
             * @throws `std::runtime_error`     If the file has a bad name table offset.
             */
            BiomxtFile(const std::string& path);

            /**
             * @brief           Destructor
             * 
             * @note            Close file stream, clear memory buffers.
             */
            ~BiomxtFile();

            /**
             * @brief           Move constructor
             * 
             * @param other     The BiomxtFile instance to move from.
             */
            BiomxtFile(BiomxtFile&& other) noexcept;

            /**
             * @brief                               Move assignment operator
             * 
             * @param other                         The resources are moved from, invalidated after the move.
             * @return BiomxtFile& 
             */
            BiomxtFile& operator=(BiomxtFile&& other) noexcept;

            /**
             * @brief                               Read a block from file
             * 
             * @param index                         The block index to read
             * @param compressed_buffer             The compressed buffer to store decompressed data
             * @param cells                         The cells to store read data
             * @throws std::runtime_error           If file is closed
             * @throws std::invalid_argument        If data type mismatch
             * @throws std::out_of_range            If block index exceeds block count
             * @throws std::runtime_error           If compress failed
             * @throws std::runtime_error           If read data from file failed
             */
            void read_block(uint32_t index, std::vector<char>& buffer);

            /**
             * @brief                               Read a row from file
             * 
             * @param row_index                     The row index to read
             * @param buffer                        The buffer to store read data
             * @throws std::runtime_error           If file is closed
             * @throws std::out_of_range            If row index exceeds row count
             */
            void read_row_data(uint32_t row_index, std::vector<char>& buffer);

            /**
             * @brief                               Read a row from file
             * 
             * @param row_name                      The row name to read
             * @param buffer                        The buffer to store read data
             * @throws std::runtime_error           If file is closed
             * @throws std::invalid_argument        If data type mismatch
             * @throws std::out_of_range            If row index exceeds row count
             */
            void read_row_data(std::string row_name, std::vector<char>& buffer);

            /**
             * @brief                               Read a row from file
             * 
             * @param row_index                     The row index to read
             * @param func                          The function to process read data
             * @return auto                         The result of func
             * @throws std::runtime_error           If file is closed
             * @throws std::invalid_argument        If data type mismatch
             * @throws std::out_of_range            If row index exceeds row count
             * @note                                The function func must accept a biomxt::Cells object as parameter.
             */
            template <typename F> auto read_row(uint32_t row_index, F&& func) {
                std::vector<char> buffer(_header.ncol * biomxt::size_of_dtype(_header.dtype));
                biomxt::BiomxtFile::read_row_data(row_index, buffer);
                switch (_header.dtype) {
                    case biomxt::DataType::INT16:
                        return func(biomxt::Cells<int16_t>(buffer));
                    case biomxt::DataType::INT32:
                        return func(biomxt::Cells<int32_t>(buffer));
                    case biomxt::DataType::INT64:
                        return func(biomxt::Cells<int64_t>(buffer));
                    case biomxt::DataType::FLOAT32:
                        return func(biomxt::Cells<float>(buffer));
                    case biomxt::DataType::FLOAT64:
                        return func(biomxt::Cells<double>(buffer));
                    default:
                        throw std::invalid_argument("biomxt::BiomxtFile::read_row: unsupported data type [" + std::to_string(_header.dtype) + "]");
                }
            }

            /**
             * @brief                               Read a column from file
             * 
             * @param column_index                  The column index to read
             * @param buffer                        The buffer to store read data
             * @throws std::runtime_error           If file is closed
             * @throws std::out_of_range            If column index exceeds column count
             */
            void read_column_data(uint32_t column_index, std::vector<char>& buffer);

            /**
             * @brief                               Read a column from file
             * 
             * @param column_name                   The column name to read
             * @param buffer                        The buffer to store read data
             * @throws std::runtime_error           If file is closed
             * @throws std::invalid_argument        If column name not found
             * @throws std::out_of_range            If column index exceeds column count
             */
            void read_column_data(std::string column_name, std::vector<char>& buffer);

            /**
             * @brief Get row names
             * 
             * @return const std::vector<std::string>& 
             * @throws std::runtime_error If the file has been closed.
             */
            const std::vector<std::string>& get_row_names() const;

            /**
             * @brief Get row names for given indices
             * 
             * @param row_indices Row indices
             * @return std::vector<std::string> Row names
             * @throws std::runtime_error If the file has been closed.
             * @throws std::runtime_error If any index is out of range.
             */
            std::vector<std::string> get_row_names(const std::vector<uint32_t>& row_indices) const;

            /**
             * @brief Get column names
             * 
             * @return const std::vector<std::string>& Column names
             * @throws std::runtime_error If the file has been closed.
             */
            const std::vector<std::string>& get_column_names() const;

            /**
             * @brief Get column names for given indices
             * 
             * @param column_indices Column indices
             * @return std::vector<std::string> Column names
             * @throws std::runtime_error If the file has been closed.
             * @throws std::runtime_error If any index is out of range.
             */
            std::vector<std::string> get_column_names(const std::vector<uint32_t>& column_indices) const;
            
            /**
             * @brief Get row indices for given names
             * 
             * @param row_names Row names
             * @return std::vector<uint32_t> Row indices
             * @throws std::runtime_error If the file has been closed.
             * @throws std::runtime_error If any name is not found.
             */
            std::vector<uint32_t> get_row_indices(const std::vector<std::string>& row_names) const;
            
            /**
             * @brief Get column indices for given names
             * 
             * @param column_names Column names
             * @return std::vector<uint32_t> Column indices
             * @throws std::runtime_error If the file has been closed.
             * @throws std::runtime_error If any name is not found.
             */
            std::vector<uint32_t> get_column_indices(const std::vector<std::string>& column_names) const;
            
            /**
             * @brief Get the file header.
             * 
             * @return `biomxt::FileHeader&` The file header.
             * @throws std::runtime_error If the file has been closed.
             */
            biomxt::FileHeader& get_header();

            /**
             * @brief Close the file stream and release resources.
             */
            void close();

             /**
             * @brief Get the maximum compressed block size.
             * 
             * @return uint32_t The maximum compressed block size.
             */
            uint32_t get_max_compressed_block_size() const;

             /**
             * @brief Get the maximum uncompressed block size.
             * 
             * @return uint32_t The maximum uncompressed block size.
             */
            uint32_t get_max_uncompressed_block_size() const;

            /**
             * @brief Get the memory limit of the block cache.
             * 
             * @return uint32_t The memory limit of the block cache.
             */
            uint32_t get_block_cache_memory_limit() const;

        private:
            std::ifstream _ifile;
            FileHeader _header;
            std::vector<IndexEntry> _block_table;
            std::vector<std::string> _row_names;
            std::vector<std::string> _column_names;
            std::unordered_map<std::string, uint32_t> _row_map;
            std::unordered_map<std::string, uint32_t> _column_map;
            uint32_t _max_compressed_block_size = 0;
            uint32_t _max_uncompressed_block_size = 0;
            std::unique_ptr<biomxt::BlockCache> _owned_block_cache = nullptr;
            BlockCache* _block_cache = nullptr;

            /**
             * @brief Close the file stream, clear data and release memory.
             */
            void _release_resources() {
                // Close the file if it is open
                if (_ifile.is_open()) _ifile.close();
                
                // Clear the chunk table and release memory
                _block_table.clear();
                _block_table.shrink_to_fit();
                
                // Clear row and column names and release memory
                _row_names.clear();
                _row_names.shrink_to_fit();
                
                // Clear column names and release memory
                _column_names.clear();
                _column_names.shrink_to_fit();

                // Release memory of mapping of row and column names by swapping with empty maps
                std::unordered_map<std::string, uint32_t>().swap(_row_map);
                std::unordered_map<std::string, uint32_t>().swap(_column_map);
            }
    };
}
