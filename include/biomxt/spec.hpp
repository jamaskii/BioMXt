#pragma once
#include <cstdint>
#include <iostream>
#include <vector>
#include "biomxt/uuid.hpp"


#pragma pack(push, 1)

#define BIOMXT_FILE_VERSION 0x0001

namespace biomxt {

    /**
     * @brief Data type enum. 矩阵的数据类型.
     */
    enum DataType : uint8_t {
        /**
         * @brief Unknown data type, which is invalid. 未知数据类型, 该值非法.
         */
        UNKNOWN = 0,

        /**
         * @brief 16-bit signed integer. 16位有符号整数.
         */
        INT16 = 1,

        /**
         * @brief 32-bit signed integer. 32位有符号整数.
         */
        INT32 = 2,

        /**
         * @brief 64-bit signed integer. 64位有符号整数.
         */
        INT64 = 3,

        /**
         * @brief 32-bit floating point number. 32位浮点数.
         */
        FLOAT32 = 4,

        /**
         * @brief 64-bit floating point number. 64位浮点数.
         */
        FLOAT64 = 5
    };

    /**
     * @brief Convert data type enum to string. 将数据类型枚举转换为字符串.
     * @param dtype Data type enum. 数据类型枚举.
     * @return std::string String representation of data type. 数据类型的字符串表示.
     */
    inline std::string dtype_to_string(DataType dtype) {
        switch (dtype) {
            case INT16: return "int16";
            case INT32: return "int32";
            case INT64: return "int64";
            case FLOAT32: return "float32";
            case FLOAT64: return "float64";
            default: return "unknown";
        }
    }

    /**
     * @brief Convert string to data type enum. 将字符串转换为数据类型枚举.
     * @param type String representation of data type. 数据类型的字符串表示.
     * @return DataType Data type enum. 数据类型枚举.
     */
    inline DataType dtype_from_string(const std::string& type) {
        if (type == "int16") return DataType::INT16;
        if (type == "int32") return DataType::INT32;
        if (type == "int64") return DataType::INT64;
        if (type == "float32" || type == "float") return DataType::FLOAT32;
        if (type == "float64" || type == "double") return DataType::FLOAT64;
        return DataType::UNKNOWN;
    }

    inline uint32_t size_of_dtype(DataType dtype) {
        switch (dtype) {
            case INT16: return sizeof(int16_t);
            case INT32: return sizeof(int32_t);
            case INT64: return sizeof(int64_t);
            case FLOAT32: return sizeof(float);
            case FLOAT64: return sizeof(double);
            default: return 0;
        }
    }

    /**
     * @brief Get data type enum from type. 从类型获取数据类型枚举.
     * @tparam T Type. 类型.
     */
    template<typename T>
               struct dtype_from_type          { static constexpr DataType value = DataType::UNKNOWN; static constexpr bool valid = false; };
    template<> struct dtype_from_type<int16_t> { static constexpr DataType value = DataType::INT16; static constexpr bool valid = true; };
    template<> struct dtype_from_type<int32_t> { static constexpr DataType value = DataType::INT32; static constexpr bool valid = true; };
    template<> struct dtype_from_type<int64_t> { static constexpr DataType value = DataType::INT64; static constexpr bool valid = true; };
    template<> struct dtype_from_type<float>   { static constexpr DataType value = DataType::FLOAT32; static constexpr bool valid = true; };
    template<> struct dtype_from_type<double>  { static constexpr DataType value = DataType::FLOAT64; static constexpr bool valid = true; };

    /**
     * @brief Get type from data type enum. 从数据类型枚举获取类型.
     * @tparam DType Data type enum. 数据类型枚举.
     */
    template <DataType DType>
                struct type_from_dtype;
    template <> struct type_from_dtype<DataType::INT16>   { using type = int16_t; };
    template <> struct type_from_dtype<DataType::INT32>   { using type = int32_t; };
    template <> struct type_from_dtype<DataType::INT64>   { using type = int64_t; };
    template <> struct type_from_dtype<DataType::FLOAT32> { using type = float; };
    template <> struct type_from_dtype<DataType::FLOAT64> { using type = double; };

    /**
     * @brief Compress algorithm enum. 压缩算法枚举.
     */
    enum CompressAlgo : uint8_t {
        /**
         * @brief Zstandard compression algorithm. Zstandard压缩算法.
         */
        ZSTD = 0,

        /**
         * @brief Gzip compression algorithm. Gzip压缩算法.
         */
        GZIP = 1,

        /**
         * @brief LZ4 compression algorithm. LZ4压缩算法.
         */
        LZ4 = 2
    };

    /**
     * @brief Convert compress algorithm enum to string. 将压缩算法枚举转换为字符串.
     * @param algo Compress algorithm enum. 压缩算法枚举.
     * @return std::string String representation of compress algorithm. 压缩算法的字符串表示.
     */
    inline std::string algo_to_string(CompressAlgo algo) {
        switch (algo) {
            case ZSTD: return "zstd";
            case GZIP: return "gzip";
            case LZ4: return "lz4";
            default: return "unknown";
        }
    }

    /**
     * @brief Convert string to compress algorithm enum. 将字符串转换为压缩算法枚举.
     * @param algo String representation of compress algorithm. 压缩算法的字符串表示.
     * @return CompressAlgo Compress algorithm enum. 压缩算法枚举.
     */
    inline CompressAlgo algo_from_string(const std::string& algo) {
        if (algo == "zstd") return CompressAlgo::ZSTD;
        if (algo == "gzip") return CompressAlgo::GZIP;
        if (algo == "lz4") return CompressAlgo::LZ4;
        return CompressAlgo::ZSTD;
    }

    /**
     * @brief File header struct.
     */
    struct FileHeader {
        char magic[4] = {'B', 'M', 'X', 't'};

        uint16_t version = BIOMXT_FILE_VERSION;

        DataType dtype = DataType::FLOAT32;

        CompressAlgo algo = CompressAlgo::ZSTD;

        uint32_t nrow;

        uint32_t ncol;

        uint32_t block_width;

        uint32_t block_height;

        uint32_t block_count;

        uint32_t padding1 = 0;

        uint64_t block_table_offset;

        uint64_t name_table_offset;

        UUID uuid;
    };

    /**
     * @brief Print file header.
     * @param header File header to be printed.
     */
    inline void print_bmxt_header(const biomxt::FileHeader& header) {
        std::cout << "Magic: \t\t\t" << std::string_view(header.magic, 4) << std::endl;
        std::cout << "Version: \t\t" << header.version << std::endl;
        std::cout << "Data type: \t\t" << biomxt::dtype_to_string(header.dtype) << std::endl;
        std::cout << "Compress algorithm: \t" << biomxt::algo_to_string(header.algo) << std::endl;
        std::cout << "Row counts: \t\t" << header.nrow << std::endl;
        std::cout << "Column counts: \t\t" << header.ncol << std::endl;
        std::cout << "Block width: \t\t" << header.block_width << std::endl;
        std::cout << "Block height: \t\t" << header.block_height << std::endl;
        std::cout << "Block count: \t\t" << header.block_count << std::endl;
        std::cout << "Block table offset: \t" << header.block_table_offset << std::endl;
        std::cout << "Name table offset: \t" << header.name_table_offset << std::endl;
    }

    /**
     * @brief Index entry struct.
     */
    struct IndexEntry {
        /**
         * @brief Offset.
         */
        uint64_t offset;

        /**
         * @brief Size.
         */
        uint32_t size;

        /**
         * @brief Uncompressed size.
         */
        uint32_t raw_size;
    };

    template <typename T>
    class Cells {
    public:
        // 构造函数：关联原始字节缓冲区
        Cells(const std::vector<char>& raw_buffer) 
            : _ptr(reinterpret_cast<const T*>(raw_buffer.data())),
              _size(raw_buffer.size() / sizeof(T)) {}

        // 像 vector 一样访问
        const T& operator[](size_t index) const { return _ptr[index]; }
        const T& at(size_t index) const {
            if (index >= _size) throw std::out_of_range("Cells index out of range");
            return _ptr[index];
        }

        // 迭代器支持 (这样就能用 for(auto x : cells) 了)
        const T* begin() const { return _ptr; }
        const T* end() const { return _ptr + _size; }

        // 基础信息
        size_t size() const { return _size; }
        bool empty() const { return _size == 0; }
        const T* data() const { return _ptr; }

    private:
        const T* _ptr;
        size_t _size;
    };

}

#pragma pack(pop)