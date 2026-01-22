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
#include "./cache/block_cache.hpp"
#include "./struct/cells.hpp"


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
            BiomxtFile(const std::string& path, BlockCache* block_cache) {
                // Open file in binary mode for reading
                _ifile.open(path, std::ios::binary);
                if (!_ifile.is_open()) {
                    throw std::runtime_error("biomxt::BiomxtFile: Cannot open mmxt file: " + path);
                }

                // If cache_entries is nullptr, use the internal cache_entries
                if (block_cache) {
                    _block_cache = block_cache;
                } else {
                    _owned_block_cache = std::make_unique<BlockCache>();
                    _block_cache = _owned_block_cache.get();
                }

                // Get file size for checking
                _ifile.seekg(0, std::ios::end);
                std::streamsize file_size = _ifile.tellg();
                _ifile.seekg(0, std::ios::beg);

                // Read file header
                if (file_size < sizeof(biomxt::FileHeader)) {
                    throw std::runtime_error("biomxt::BiomxtFile: Corrupted file: bad header size");
                }
                _ifile.read(reinterpret_cast<char*>(&_header), sizeof(biomxt::FileHeader));

                // Check magic
                if (std::string(_header.magic, 4) != "BMXt") {
                    throw std::runtime_error("biomxt::BiomxtFile: Corrupted file: bad magic: " + std::string(_header.magic, 4));
                }

                // Read block table
                if (_header.block_table_offset >= file_size) {
                    throw std::runtime_error("biomxt::BiomxtFile: Corrupted file: block table offset [" + std::to_string(_header.block_table_offset) + "] exceeds file size [" + std::to_string(file_size) + "]");
                }
                _ifile.seekg(_header.block_table_offset, std::ios::beg);
                _block_table.resize(_header.block_count);
                _ifile.read(reinterpret_cast<char*>(_block_table.data()), _header.block_count * sizeof(biomxt::IndexEntry));

                // Calculate max compressed block size
                for (const auto& block_index : _block_table) {
                    _max_compressed_block_size = std::max(_max_compressed_block_size, block_index.size);
                    _max_uncompressed_block_size = std::max(_max_uncompressed_block_size, block_index.raw_size);
                }
                if (block_cache == nullptr) _block_cache->set_memory_limit(std::max(_header.ncol / _header.block_width, _header.nrow / _header.block_height) * (_max_uncompressed_block_size + sizeof(biomxt::CacheEntry)));

                // Read names table
                if (_header.name_table_offset >= file_size) {
                    throw std::runtime_error("Corrupted file: names table offset [" + std::to_string(_header.name_table_offset) + "] exceeds file size [" + std::to_string(file_size) + "]");
                }
                _ifile.seekg(_header.name_table_offset, std::ios::beg);
                std::vector<biomxt::IndexEntry> name_table(_header.nrow + _header.ncol);
                name_table.resize(_header.nrow + _header.ncol);
                _ifile.read(reinterpret_cast<char*>(name_table.data()), (_header.nrow + _header.ncol) * sizeof(biomxt::IndexEntry));

                // Read row names and build map
                _row_names.resize(_header.nrow);
                _row_map.reserve(_header.nrow);
                for (uint32_t i = 0; i < _header.nrow; ++i) {
                    _ifile.seekg(name_table[i].offset, std::ios::beg);
                    _row_names[i].resize(name_table[i].size);
                    _ifile.read(&_row_names[i][0], name_table[i].size);
                    _row_map[_row_names[i]] = i;
                }

                // Read column names and build map
                _column_names.resize(_header.ncol);
                _column_map.reserve(_header.ncol);
                for (uint32_t i = 0; i < _header.ncol; ++i) {
                    uint32_t idx = _header.nrow + i;
                    _ifile.seekg(name_table[idx].offset, std::ios::beg);
                    _column_names[i].resize(name_table[idx].size);
                    _ifile.read(&_column_names[i][0], name_table[idx].size);
                    _column_map[_column_names[i]] = i;
                }
            }

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
            BiomxtFile(const std::string& path) : BiomxtFile(path, nullptr) {}

            /**
             * @brief           Destructor
             * 
             * @note            Close file stream, clear memory buffers.
             */
            ~BiomxtFile() { _release_resources(); }

            /**
             * @brief           Move constructor
             * 
             * @param other     The BiomxtFile instance to move from.
             */
            BiomxtFile(BiomxtFile&& other) noexcept { *this = std::move(other); }

            /**
             * @brief                               Move assignment operator
             * 
             * @param other                         The resources are moved from, invalidated after the move.
             * @return BiomxtFile& 
             */
            BiomxtFile& operator=(BiomxtFile&& other) noexcept {
                if (this != &other) {
                    // Release old resources
                    _release_resources();
                    
                    // Move resources from other to this
                    _ifile = std::move(other._ifile);
                    _header = other._header;
                    _block_table = std::move(other._block_table);
                    _row_names = std::move(other._row_names);
                    _column_names = std::move(other._column_names);
                    _row_map = std::move(other._row_map);
                    _column_map = std::move(other._column_map);
                    _max_compressed_block_size = other._max_compressed_block_size;
                    _max_uncompressed_block_size = other._max_uncompressed_block_size;

                    // Exchange block cache
                    _owned_block_cache = std::move(other._owned_block_cache);
                    _block_cache = other._block_cache;
                    
                    // Set other to safty state
                    other._header = {}; 
                    other._block_cache = nullptr;
                }
                return *this;
            }

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
            void read_block(uint32_t index, std::vector<char>& buffer) {
                // Check file is closed
                if (!_ifile.is_open()) {
                    throw std::runtime_error("biomxt::BiomxtFile::read_block: file is closed");
                }

                // Check index range
                if (index >= _header.block_count) {
                    throw std::out_of_range("biomxt::BiomxtFile::read_block: block index [" + std::to_string(index) + "] exceeds block count [" + std::to_string(_header.block_count) + "]");
                }

                // Check buffer size
                const auto& block_index = _block_table[index];
                if (buffer.size() != block_index.raw_size) buffer.resize(block_index.raw_size);
                std::vector<char> compressed_buffer(block_index.size);

                // Check cache
                biomxt::BlockKey key = {index, _header.uuid};
                if (_block_cache->get_block_data(key, buffer, 0, block_index.raw_size)) return;

                // Read from file
                _ifile.seekg(block_index.offset, std::ios::beg);
                if (!_ifile.read(compressed_buffer.data(), block_index.size)) {
                    throw std::runtime_error("biomxt::BiomxtFile::read_block: read block [" + std::to_string(index) + "] data from file failed");
                }
                
                // Decompress
                size_t decompressed_size = 0;
                switch (_header.algo) {
                    case biomxt::CompressAlgorithm::ZSTD:
                        decompressed_size = ZSTD_decompress(
                            buffer.data(),                  // target addr
                            block_index.raw_size,           // target size
                            compressed_buffer.data(),       // source addr
                            block_index.size                // source size
                        );
                        if (ZSTD_isError(decompressed_size)) {
                            throw std::runtime_error("biomxt::BiomxtFile::read_block: ZSTD_decompress error [" + std::string(ZSTD_getErrorName(decompressed_size)) + "]");
                        }
                        break;
                    default:
                        throw std::invalid_argument("biomxt::BiomxtFile::read_block: unsupported compression algorithm [" + std::to_string(_header.algo) + "]");
                }

                // Cache block data
                std::vector<char> cache_data(block_index.raw_size);
                std::memcpy(cache_data.data(), buffer.data(), block_index.raw_size);
                _block_cache->insert({index, _header.uuid}, std::move(cache_data));
                
            }

            /**
             * @brief                               Read a row from file
             * 
             * @param row_index                     The row index to read
             * @param buffer                        The buffer to store read data
             * @throws std::runtime_error           If file is closed
             * @throws std::out_of_range            If row index exceeds row count
             */
            void read_row_data(uint32_t row_index, std::vector<char>& buffer) {
                // Check file is closed
                if (!_ifile.is_open()) {
                    throw std::runtime_error("biomxt::BiomxtFile::read_row_data: file is closed");
                }
                
                // Check row index range
                if (row_index >= _header.nrow) {
                    throw std::out_of_range("biomxt::BiomxtFile::read_row_data: row index [" + std::to_string(row_index) + "] exceeds row count [" + std::to_string(_header.nrow) + "]");
                }
                
                // Prepare result container
                uint32_t cell_size = biomxt::size_of_dtype(_header.dtype);
                buffer.resize(_header.ncol * cell_size);

                // Calculate block pos
                uint32_t block_pos_y = row_index / _header.block_height; // Block's row index
                uint32_t row_in_block = row_index % _header.block_height;  // Target row index inner block

                // How many block in horizontal direction
                uint32_t block_max_x = (_header.ncol + _header.block_width - 1) / _header.block_width;

                std::vector<char> block_buffer(_header.block_width * _header.block_height * cell_size);

                // Traverse all blocks in horizontal direction
                for (uint32_t block_pos_x = 0; block_pos_x < block_max_x; ++block_pos_x) {
                    uint32_t block_idx = (uint32_t)block_pos_y * block_max_x + block_pos_x;
                    
                    // Read block
                    this->read_block(block_idx, block_buffer);

                    // Calculate actual block size
                    uint32_t actual_block_width = std::min(_header.block_width, _header.ncol - block_pos_x * _header.block_width);

                    // Fetch target inner block row
                    const char* row_start = block_buffer.data() + (row_in_block * actual_block_width * cell_size);
                    std::copy(row_start, row_start + actual_block_width * cell_size, buffer.begin() + (block_pos_x * _header.block_width * cell_size));
                }

            }

            /**
             * @brief                               Read a row from file
             * 
             * @param row_name                      The row name to read
             * @param buffer                        The buffer to store read data
             * @throws std::runtime_error           If file is closed
             * @throws std::invalid_argument        If data type mismatch
             * @throws std::out_of_range            If row index exceeds row count
             */
            void read_row_data(std::string row_name, std::vector<char>& buffer) {
                auto it = _row_map.find(row_name);
                if (it == _row_map.end()) {
                    throw std::invalid_argument("biomxt::BiomxtFile::read_row_data: row name [" + row_name + "] not found");
                }
                this->read_row_data(it->second, buffer);
            }

            template <typename F> auto read_row(uint32_t row_index, F&& func) {
                std::vector<char> buffer(_header.ncol * biomxt::size_of_dtype(_header.dtype));
                this->read_row_data(row_index, buffer);
                switch (_header.dtype) {
                    case INT16:
                        return func(biomxt::Cells<int16_t>(buffer));
                    case INT32:
                        return func(biomxt::Cells<int32_t>(buffer));
                    case INT64:
                        return func(biomxt::Cells<int64_t>(buffer));
                    case FLOAT32:
                        return func(biomxt::Cells<float>(buffer));
                    case FLOAT64:
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
            void read_column_data(uint32_t column_index, std::vector<char>& buffer) {
                // Check file is closed
                if (!_ifile.is_open()) {
                    throw std::runtime_error("biomxt::BiomxtFile::read_column_data: file is closed");
                }
                
                // Check row index range
                if (column_index >= _header.ncol) {
                    throw std::out_of_range("biomxt::BiomxtFile::read_column_data: column index [" + std::to_string(column_index) + "] exceeds column count [" + std::to_string(_header.ncol) + "]");
                }
                
                // Prepare result container
                uint32_t cell_size = biomxt::size_of_dtype(_header.dtype);
                buffer.resize(_header.nrow * cell_size);
                // Prepare block buffer
                std::vector<char> block_buffer(_header.block_width * _header.block_height * cell_size);

                // Calculate block pos
                uint32_t block_pos_x = column_index / _header.block_width; // Block's column index
                uint32_t col_in_block = column_index % _header.block_width;  // Target column index inner block

                // How many block in vertical direction
                uint32_t block_max_x = (_header.ncol + _header.block_width - 1) / _header.block_width;
                uint32_t block_max_y = (_header.nrow + _header.block_height - 1) / _header.block_height;

                // Traverse all blocks in vertical direction
                for (uint32_t block_pos_y = 0; block_pos_y < block_max_y; ++block_pos_y) {
                    uint32_t block_idx = (uint32_t)block_pos_y * block_max_x + block_pos_x;
                    
                    // Read block
                    this->read_block(block_idx, block_buffer);

                    // Calculate actual block size
                    uint32_t actual_block_width = std::min(_header.block_width, _header.ncol - block_pos_x * _header.block_width);
                    uint32_t actual_block_height = block_buffer.size() / cell_size / actual_block_width;

                    // Fetch target inner block col
                    char* block_ptr = block_buffer.data() + col_in_block * cell_size; // Target column's first row in block
                    uint64_t cell_offset = (uint64_t)block_pos_y * _header.block_height * cell_size; // Calculate cell offset in result cells

                    for (uint32_t i = 0; i < actual_block_height; ++i) {
                        std::memcpy(
                            buffer.data() + cell_offset + i * cell_size, 
                            block_ptr, 
                            cell_size);
                        block_ptr += actual_block_width * cell_size; // Jump to next row inner block
                    }
                }

            }

            /**
             * @brief                               Read a column from file
             * 
             * @param column_name                   The column name to read
             * @param buffer                        The buffer to store read data
             * @throws std::runtime_error           If file is closed
             * @throws std::invalid_argument        If column name not found
             * @throws std::out_of_range            If column index exceeds column count
             */
            void read_column_data(std::string column_name, std::vector<char>& buffer) {
                auto it = _column_map.find(column_name);
                if (it == _column_map.end()) {
                    throw std::invalid_argument("biomxt::BiomxtFile::read_column_data: column name [" + column_name + "] not found");
                }
                return read_column_data(it->second, buffer);
            }

            // template <typename T> bool read_sub_matrix(const std::vector<std::string>& row_names, const std::vector<std::string>& column_names, std::vector<T>& cells);
            // template <typename T> bool read_sub_matrix(std::vector<uint32_t> row_indices, const std::vector<std::string>& column_names, std::vector<T>& cells);
            // template <typename T> bool read_sub_matrix(const std::vector<std::string>& row_names, std::vector<uint32_t> column_indices, std::vector<T>& cells);
            // template <typename T> bool read_sub_matrix(std::vector<uint32_t> row_indices, std::vector<uint32_t> column_indices, std::vector<T>& cells);

            /**
             * @brief Get row names
             * 
             * @return const std::vector<std::string>& 
             * @throws std::runtime_error If the file has been closed.
             */
            const std::vector<std::string>& get_row_names() const {
                if (!_ifile.is_open()) {
                    throw std::runtime_error("biomxt::BiomxtFile::get_row_names: File has been closed.");
                }
                return _row_names;
            }
            /**
             * @brief Get row names for given indices
             * 
             * @param row_indices Row indices
             * @return std::vector<std::string> Row names
             * @throws std::runtime_error If the file has been closed.
             * @throws std::runtime_error If any index is out of range.
             */
            std::vector<std::string> get_row_names(const std::vector<uint32_t>& row_indices) const {
                if (!_ifile.is_open()) {
                    throw std::runtime_error("biomxt::BiomxtFile::get_row_names: File has been closed.");
                }
                // Reserve
                std::vector<std::string> results;
                results.reserve(row_indices.size()); 
                
                // Fill
                for (uint32_t idx : row_indices) {
                    if (idx < _header.nrow) {
                        results.push_back(_row_names[idx]);
                    } else {
                        throw std::runtime_error("biomxt::BiomxtFile::get_row_names: Row index out of range: " + std::to_string(idx));
                    }
                }
                return results;
            }

            /**
             * @brief Get column names
             * 
             * @return const std::vector<std::string>& Column names
             * @throws std::runtime_error If the file has been closed.
             */
            const std::vector<std::string>& get_column_names() const { 
                if (!_ifile.is_open()) {
                    throw std::runtime_error("biomxt::BiomxtFile::get_column_names: File has been closed.");
                }
                return _column_names;
            }
            /**
             * @brief Get column names for given indices
             * 
             * @param column_indices Column indices
             * @return std::vector<std::string> Column names
             * @throws std::runtime_error If the file has been closed.
             * @throws std::runtime_error If any index is out of range.
             */
            std::vector<std::string> get_column_names(const std::vector<uint32_t>& column_indices) const {
                if (!_ifile.is_open()) {
                    throw std::runtime_error("biomxt::BiomxtFile::get_column_names: File has been closed.");
                }
                // Reserve
                std::vector<std::string> results;
                results.reserve(column_indices.size()); 
                
                // Fill
                for (uint32_t idx : column_indices) {
                    if (idx < _header.ncol) {
                        results.push_back(_column_names[idx]);
                    } else {
                        throw std::runtime_error("biomxt::BiomxtFile::get_column_names: Column index out of range: " + std::to_string(idx));
                    }
                }
                return results;
            }
            
            /**
             * @brief Get row indices for given names
             * 
             * @param row_names Row names
             * @return std::vector<uint32_t> Row indices
             * @throws std::runtime_error If the file has been closed.
             * @throws std::runtime_error If any name is not found.
             */
            std::vector<uint32_t> get_row_indices(const std::vector<std::string>& row_names) const {
                if (!_ifile.is_open()) {
                    throw std::runtime_error("biomxt::BiomxtFile::get_row_indices: File has been closed.");
                }

                std::vector<uint32_t> results;
                // Reserve
                results.reserve(row_names.size());

                for (const auto& name : row_names) {
                    // Locate via map
                    auto it = _row_map.find(name);
                    
                    // Not found
                    if (it == _row_map.end()) {
                        throw std::runtime_error("biomxt::BiomxtFile::get_row_indices: Row name not found: " + name);
                    }

                    // Found
                    results.push_back(it->second);
                }
                
                return results;
            }
            
            /**
             * @brief Get column indices for given names
             * 
             * @param column_names Column names
             * @return std::vector<uint32_t> Column indices
             * @throws std::runtime_error If the file has been closed.
             * @throws std::runtime_error If any name is not found.
             */
            std::vector<uint32_t> get_column_indices(const std::vector<std::string>& column_names) const {
                if (!_ifile.is_open()) {
                    throw std::runtime_error("biomxt::BiomxtFile::get_column_indices: File has been closed.");
                }
                // Reserve
                std::vector<uint32_t> results;
                results.reserve(column_names.size()); 
                
                // Fill
                for (const auto& name : column_names) {
                    // Locate via map
                    auto it = _column_map.find(name);
                    
                    // Not found
                    if (it == _column_map.end()) {
                        throw std::runtime_error("biomxt::BiomxtFile::get_column_indices: Column name not found: " + name);
                    }

                    // Found
                    results.push_back(it->second);
                }
                
                return results;
            }

            
            /**
             * @brief Get the file header.
             * 
             * @return `biomxt::FileHeader&` The file header.
             * @throws std::runtime_error If the file has been closed.
             */
            biomxt::FileHeader& get_header() { 
                if (!_ifile.is_open()) {
                    throw std::runtime_error("biomxt::BiomxtFile::get_header: File has been closed.");
                }
                return _header;
            }

            /**
             * @brief Close the file stream and release resources.
             */
            void close() { _release_resources(); }

             /**
             * @brief Get the maximum compressed block size.
             * 
             * @return uint32_t The maximum compressed block size.
             */
            uint32_t get_max_compressed_block_size() const { return _max_compressed_block_size; }

             /**
             * @brief Get the maximum uncompressed block size.
             * 
             * @return uint32_t The maximum uncompressed block size.
             */
            uint32_t get_max_uncompressed_block_size() const { return _max_uncompressed_block_size; }

            uint32_t get_block_cache_memory_limit() const { return _block_cache->get_memory_limit(); }

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