#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include "biomxt/csv_parser.hpp"
#include "biomxt/spec.hpp"
#include "zstd.h"
#include <functional>


namespace biomxt {

    bool chunk_flush(
        std::vector<float>& chunk, 
        std::ofstream& out, 
        std::vector<biomxt::IndexEntry>& chunk_indices, 
        biomxt::CompressAlgo algo);

    biomxt::FileHeader csv_to_bmxt(
        const std::string& input_file, 
        const std::string& output_file, 
        bool first_col_as_rownames, 
        uint64_t chunk_size, 
        char separation, 
        biomxt::DataType dtype, 
        biomxt::CompressAlgo algo, 
        std::string& error, 
        std::vector<std::string>& warnings);

    class BiomxtFile {
        public:
            /**
             * @brief Constructor
             * 
             * @param path The path to the Biomxt file to open.
             * @throws std::runtime_error If the file cannot be opened.
             * @throws std::runtime_error If the file has a bad header.
             * @throws std::runtime_error If the file has a bad magic.
             * @throws std::runtime_error If the file has a bad chunk table offset.
             * @throws std::runtime_error If the file has a bad names table offset.
             */
            BiomxtFile(const std::string& path) {
                // Open file in binary mode for reading
                _ifile.open(path, std::ios::binary);
                if (!_ifile.is_open()) {
                    throw std::runtime_error("Cannot open Biomxt file: " + path);
                }

                // Get file size for checking
                _ifile.seekg(0, std::ios::end);
                std::streamsize file_size = _ifile.tellg();
                _ifile.seekg(0, std::ios::beg);

                // Read file header
                if (file_size < sizeof(biomxt::FileHeader)) {
                    throw std::runtime_error("Corrupted file: bad header size");
                }
                _ifile.read(reinterpret_cast<char*>(&_header), sizeof(biomxt::FileHeader));

                // Check magic
                if (std::string(_header.magic, 4) != "BMXt") {
                    throw std::runtime_error("Corrupted file: bad magic: " + std::string(_header.magic, 4));
                }

                // Read Chunk Table
                if (_header.chunk_table_offset >= file_size) {
                    throw std::runtime_error("Corrupted file: chunk table offset [" + std::to_string(_header.chunk_table_offset) + "] exceeds file size [" + std::to_string(file_size) + "]");
                }
                _ifile.seekg(_header.chunk_table_offset, std::ios::beg);
                _chunk_table.resize(get_chunk_count());
                _ifile.read(reinterpret_cast<char*>(_chunk_table.data()), get_chunk_count() * sizeof(biomxt::IndexEntry));

                // Read names table
                if (_header.names_table_offset >= file_size) {
                    throw std::runtime_error("Corrupted file: names table offset [" + std::to_string(_header.names_table_offset) + "] exceeds file size [" + std::to_string(file_size) + "]");
                }
                _ifile.seekg(_header.names_table_offset, std::ios::beg);
                std::vector<biomxt::IndexEntry> names_table(_header.nrow + _header.ncolumn);
                names_table.resize(_header.nrow + _header.ncolumn);
                _ifile.read(reinterpret_cast<char*>(names_table.data()), (_header.nrow + _header.ncolumn) * sizeof(biomxt::IndexEntry));

                // Read row names and build map
                _row_names.resize(_header.nrow);
                _row_map.reserve(_header.nrow); // 预分配哈希表空间，减少 rehash
                for (uint64_t i = 0; i < _header.nrow; ++i) {
                    _ifile.seekg(names_table[i].offset, std::ios::beg);
                    _row_names[i].resize(names_table[i].size);
                    _ifile.read(&_row_names[i][0], names_table[i].size);
                    _row_map[_row_names[i]] = i;
                }

                // Read column names and build map
                _column_names.resize(_header.ncolumn);
                _column_map.reserve(_header.ncolumn);
                for (uint64_t i = 0; i < _header.ncolumn; ++i) {
                    uint64_t idx = _header.nrow + i;
                    _ifile.seekg(names_table[idx].offset, std::ios::beg);
                    _column_names[i].resize(names_table[idx].size);
                    _ifile.read(&_column_names[i][0], names_table[idx].size);
                    _column_map[_column_names[i]] = i;
                }
            }

            /**
             * @brief Destructor
             * 
             * @note Releases all resources held by the BiomxtFile instance,
             *       including the file stream and memory buffers.
             */
            ~BiomxtFile() { _release_resources(); }

            /**
             * @brief Move constructor
             * 
             * @param other The BiomxtFile instance to move from.
             */
            BiomxtFile(BiomxtFile&& other) noexcept {
                *this = std::move(other);
            }

            /**
             * @brief Move assignment operator
             * 
             * @param other Where the resources are moved from, 
             *              invalidated after the move.
             * @return BiomxtFile& 
             * @note This operator moves resources from `other` to `this`, 
             *       invalidating `other`,
             *       with noexcept guarantee.
             */
            BiomxtFile& operator=(BiomxtFile&& other) noexcept {
                if (this != &other) {
                    // Release old resources
                    _release_resources();
                    
                    // Move resources from other to this
                    _ifile = std::move(other._ifile);
                    _header = other._header;
                    
                    // Move resources from other to this
                    _chunk_table = std::move(other._chunk_table);
                    _row_names = std::move(other._row_names);
                    _column_names = std::move(other._column_names);
                    
                    // Move resources from other to this
                    _row_map = std::move(other._row_map);
                    _column_map = std::move(other._column_map);
                    
                    // Set other to safty state
                    other._header = {}; 
                }
                return *this;
            }

            template <typename T> bool read_chunk(uint64_t index, std::vector<T>& cells) const {

            }

            template <typename T> bool read_row(uint64_t row_index, std::vector<T>& cells);
            template <typename T> bool read_row(std::string row_name, std::vector<T>& cells);
            template <typename T> bool read_rows(std::vector<uint64_t> row_indices, std::vector<T>& cells);
            template <typename T> bool read_rows(const std::vector<std::string>& row_names, std::vector<T>& cells);
            template <typename T> bool read_rows_stream(std::vector<uint64_t> row_indices, std::function<void(uint64_t, const std::vector<T>&)> callback);
            template <typename T> bool read_rows_stream(const std::vector<std::string>& row_names, std::function<void(uint64_t, const std::vector<T>&)> callback);

            template <typename T> bool read_column(uint64_t column_index, std::vector<T>& cells);
            template <typename T> bool read_column(std::string column_name, std::vector<T>& cells);
            template <typename T> bool read_columns(std::vector<uint64_t> column_indices, std::vector<T>& cells);
            template <typename T> bool read_columns(const std::vector<std::string>& column_names, std::vector<T>& cells);
            template <typename T> bool read_columns_stream(std::vector<uint64_t> column_indices, std::function<void(uint64_t, const std::vector<T>&)> callback);
            template <typename T> bool read_columns_stream(const std::vector<std::string>& column_names, std::function<void(uint64_t, const std::vector<T>&)> callback);

            template <typename T> bool read_block(const std::vector<std::string>& row_names, const std::vector<std::string>& column_names, std::vector<T>& cells);
            template <typename T> bool read_block(std::vector<uint64_t> row_indices, const std::vector<std::string>& column_names, std::vector<T>& cells);
            template <typename T> bool read_block(const std::vector<std::string>& row_names, std::vector<uint64_t> column_indices, std::vector<T>& cells);
            template <typename T> bool read_block(std::vector<uint64_t> row_indices, std::vector<uint64_t> column_indices, std::vector<T>& cells);

            /**
             * @brief Get row names
             * 
             * @return const std::vector<std::string>& 
             * @throws std::runtime_error If the file has been closed.
             */
            const std::vector<std::string>& get_row_names() const {
                if (!_ifile.is_open()) {
                    throw std::runtime_error("File has been closed.");
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
            std::vector<std::string> get_row_names(const std::vector<uint64_t>& row_indices) const {
                if (!_ifile.is_open()) {
                    throw std::runtime_error("File has been closed.");
                }
                // Reserve
                std::vector<std::string> results;
                results.reserve(row_indices.size()); 
                
                // Fill
                for (uint64_t idx : row_indices) {
                    if (idx < _header.nrow) {
                        results.push_back(_row_names[idx]);
                    } else {
                        throw std::runtime_error("Row index out of range: " + std::to_string(idx));
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
                    throw std::runtime_error("File has been closed.");
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
            std::vector<std::string> get_column_names(const std::vector<uint64_t>& column_indices) const {
                if (!_ifile.is_open()) {
                    throw std::runtime_error("File has been closed.");
                }
                // Reserve
                std::vector<std::string> results;
                results.reserve(column_indices.size()); 
                
                // Fill
                for (uint64_t idx : column_indices) {
                    if (idx < _header.ncolumn) {
                        results.push_back(_column_names[idx]);
                    } else {
                        throw std::runtime_error("Column index out of range: " + std::to_string(idx));
                    }
                }
                return results;
            }
            
            /**
             * @brief Get row indices for given names
             * 
             * @param row_names Row names
             * @return std::vector<uint64_t> Row indices
             * @throws std::runtime_error If the file has been closed.
             * @throws std::runtime_error If any name is not found.
             */
            std::vector<uint64_t> get_row_indices(const std::vector<std::string>& row_names) const {
                if (!_ifile.is_open()) {
                    throw std::runtime_error("File has been closed.");
                }

                std::vector<uint64_t> results;
                // Reserve
                results.reserve(row_names.size());

                for (const auto& name : row_names) {
                    // Locate via map
                    auto it = _row_map.find(name);
                    
                    // Not found
                    if (it == _row_map.end()) {
                        throw std::runtime_error("Row name not found: " + name);
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
             * @return std::vector<uint64_t> Column indices
             * @throws std::runtime_error If the file has been closed.
             * @throws std::runtime_error If any name is not found.
             */
            std::vector<uint64_t> get_column_indices(const std::vector<std::string>& column_names) const {
                if (!_ifile.is_open()) {
                    throw std::runtime_error("File has been closed.");
                }
                // Reserve
                std::vector<uint64_t> results;
                results.reserve(column_names.size()); 
                
                // Fill
                for (const auto& name : column_names) {
                    // Locate via map
                    auto it = _column_map.find(name);
                    
                    // Not found
                    if (it == _column_map.end()) {
                        throw std::runtime_error("Column name not found: " + name);
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
                    throw std::runtime_error("File has been closed.");
                }
                return _header;
            }

            /**
             * @brief Get the number of chunks in the file.
             * 
             * @return uint64_t The number of chunks in the file.
             * @throws std::runtime_error If the file has been closed.
             */
            uint64_t get_chunk_count() { 
                if (!_ifile.is_open()) {
                    throw std::runtime_error("File has been closed.");
                }
                return (_header.nrow * _header.ncolumn + _header.chunk_size - 1) / _header.chunk_size;
            }

            /**
             * @brief Close the file stream and release resources.
             */
            void close() { _release_resources(); }

        private:
            std::ifstream _ifile;
            biomxt::FileHeader _header;
            std::vector<biomxt::IndexEntry> _chunk_table;
            std::vector<std::string> _row_names;
            std::vector<std::string> _column_names;
            std::unordered_map<std::string, uint64_t> _row_map;
            std::unordered_map<std::string, uint64_t> _column_map;

            /**
             * @brief Close the file stream, clear data and release memory.
             */
            void _release_resources() {
                // Close the file if it is open
                if (_ifile.is_open()) _ifile.close();
                
                // Clear the chunk table and release memory
                _chunk_table.clear();
                _chunk_table.shrink_to_fit();
                
                // Clear row and column names and release memory
                _row_names.clear();
                _row_names.shrink_to_fit();
                
                // Clear column names and release memory
                _column_names.clear();
                _column_names.shrink_to_fit();

                // Release memory of mapping of row and column names by swapping with empty maps
                std::unordered_map<std::string, uint64_t>().swap(_row_map);
                std::unordered_map<std::string, uint64_t>().swap(_column_map);
            }
    };
        
}