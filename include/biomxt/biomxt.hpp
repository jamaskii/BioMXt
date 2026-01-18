#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include "biomxt/csv_parser.hpp"
#include "biomxt/spec.hpp"
#include "zstd.h"
#include <functional>
#include <typeinfo>
#include <algorithm>


namespace biomxt {

    const uint16_t VERSION = 1;

    /**
     * @brief 将内存块压缩并写入文件，返回对应的索引条目
     * @param src_data 待压缩数据的起始指针
     * @param uncompressed_byte_size 数据原始字节大小
     * @param out 输出文件流
     * @param algo 压缩算法
     */
    biomxt::IndexEntry compress_bytes_and_write(
        const void* src_data, 
        size_t uncompressed_byte_size, 
        std::ofstream& out, 
        std::vector<char>& compressed_buf,
        biomxt::CompressAlgo algo);

    
    /**
     * @brief 将字符串转换为指定类型的数值
     * @param str 待转换的字符串
     * @return T 转换后的数值
     * @throws std::out_of_range 如果数值超出范围
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
     * @brief Convert csv file to bmxt format. 将 CSV 文件转换为 Biomxt 文件
     * @param input_file Input file path. 输入 CSV 文件路径
     * @param output_file Output file path. 输出 Biomxt 文件路径
     * @param block_width Block width. 块的宽度
     * @param block_height Block height. 块高度
     * @param separator Separator character. CSV 文件的分隔符
     * @param algo Compression algorithm. 压缩算法
     * @param warnings Warnings info during conversion.转换过程中产生的警告信息
     * @return biomxt::FileHeader Header of bmxt file converted. 转换后的文件头信息
     * @throw std::runtime_error If the input/output file cannot be opened, or 
     *                           if the csv file contains row of inconsistent counts of cells.
     */
    template <typename T>biomxt::FileHeader csv_to_bmxt(
        const std::string& input_file, 
        const std::string& output_file, 
        uint32_t block_width, 
        uint32_t block_height, 
        char separator, 
        biomxt::CompressAlgo algo,
        std::vector<std::string>& warnings);

    class BiomxtFile {
        public:
            /**
             * @brief Constructor
             * 
             * @param path The path to the Biomxt file to open. 要打开的 Biomxt 文件路径
             * @throws std::runtime_error If the file cannot be opened. 无法打开文件
             * @throws std::runtime_error If the file has a bad header/magic/blocks table offset/names table offset. 损坏的文件头信息、魔数、块表偏移量或名称表偏移量
             */
            BiomxtFile(const std::string& path) {
                // Open file in binary mode for reading
                _ifile.open(path, std::ios::binary);
                if (!_ifile.is_open()) {
                    throw std::runtime_error("biomxt::BiomxtFile: Cannot open mmxt file: " + path);
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
                if (_header.blocks_table_offset >= file_size) {
                    throw std::runtime_error("biomxt::BiomxtFile: Corrupted file: blocks table offset [" + std::to_string(_header.blocks_table_offset) + "] exceeds file size [" + std::to_string(file_size) + "]");
                }
                _ifile.seekg(_header.blocks_table_offset, std::ios::beg);
                _blocks_table.resize(_header.block_count);
                _ifile.read(reinterpret_cast<char*>(_blocks_table.data()), _header.block_count * sizeof(biomxt::IndexEntry));

                // Read names table
                if (_header.names_table_offset >= file_size) {
                    throw std::runtime_error("Corrupted file: names table offset [" + std::to_string(_header.names_table_offset) + "] exceeds file size [" + std::to_string(file_size) + "]");
                }
                _ifile.seekg(_header.names_table_offset, std::ios::beg);
                std::vector<biomxt::IndexEntry> names_table(_header.nrow + _header.ncol);
                names_table.resize(_header.nrow + _header.ncol);
                _ifile.read(reinterpret_cast<char*>(names_table.data()), (_header.nrow + _header.ncol) * sizeof(biomxt::IndexEntry));

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
                _column_names.resize(_header.ncol);
                _column_map.reserve(_header.ncol);
                for (uint64_t i = 0; i < _header.ncol; ++i) {
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
                    _blocks_table = std::move(other._blocks_table);
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

            template <typename T> bool read_chunk(uint64_t index, std::vector<T>& cells);

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
                    if (idx < _header.ncol) {
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
             * @brief Close the file stream and release resources.
             */
            void close() { _release_resources(); }

        private:
            std::ifstream _ifile;
            biomxt::FileHeader _header;
            std::vector<biomxt::IndexEntry> _blocks_table;
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
                _blocks_table.clear();
                _blocks_table.shrink_to_fit();
                
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