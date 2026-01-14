#pragma once
#include <cstdint>

#pragma pack(push, 1)

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
    inline DataType string_to_dtype(const std::string& type) {
        if (type == "int16") return DataType::INT16;
        if (type == "int32") return DataType::INT32;
        if (type == "int64") return DataType::INT64;
        if (type == "float" || type == "float32") return DataType::FLOAT32;
        if (type == "float64") return DataType::FLOAT64;
        return DataType::UNKNOWN;
    }

    /**
     * @brief Get data type enum from type. 从类型获取数据类型枚举.
     * @tparam T Type. 类型.
     */
    template<typename T>
               struct dtype_from          { static constexpr DataType value = DataType::UNKNOWN; static constexpr bool valid = false; };
    template<> struct dtype_from<int16_t> { static constexpr DataType value = DataType::INT16; static constexpr bool valid = true; };
    template<> struct dtype_from<int32_t> { static constexpr DataType value = DataType::INT32; static constexpr bool valid = true; };
    template<> struct dtype_from<int64_t> { static constexpr DataType value = DataType::INT64; static constexpr bool valid = true; };
    template<> struct dtype_from<float>   { static constexpr DataType value = DataType::FLOAT32; static constexpr bool valid = true; };
    template<> struct dtype_from<double>  { static constexpr DataType value = DataType::FLOAT64; static constexpr bool valid = true; };

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
    inline CompressAlgo string_to_algo(const std::string& algo) {
        if (algo == "zstd") return CompressAlgo::ZSTD;
        if (algo == "gzip") return CompressAlgo::GZIP;
        if (algo == "lz4") return CompressAlgo::LZ4;
        return CompressAlgo::ZSTD;
    }

    /**
     * @brief File header struct. 文件头结构体.
     */
    struct FileHeader {
        /**
         * @brief Magic number. 魔法数.
         */
        char magic[4] = {'B', 'M', 'X', 't'};

        /**
         * @brief Version number. 版本号.
         */
        uint16_t version = 1;

        /**
         * @brief Data type. 数据类型.
         */
        DataType dtype = DataType::FLOAT32;

        /**
         * @brief Compress algorithm. 压缩算法.
         */
        CompressAlgo algo = CompressAlgo::ZSTD;

        /**
         * @brief Row counts. 行计数.
         */
        uint32_t nrow;

        /**
         * @brief Column counts. 列计数.
         */
        uint32_t ncolumn;

        /**
         * @brief Chunk size. 块大小.
         */
        uint32_t chunk_size;

        /**
         * @brief Padding. 填充.
         */
        uint8_t padding1[4] = {0};

        /**
         * @brief Chunk table offset. 块表偏移.
         */
        uint64_t chunk_table_offset;

        /**
         * @brief Names table offset. 名称表偏移.
         */
        uint64_t names_table_offset;

        /**
         * @brief Padding. 填充.
         */
        uint8_t padding2[24] = {0};
    };

    /**
     * @brief Print file header. 打印文件头.
     * @param header File header. 文件头.
     */
    inline void print_bmxt_header(const biomxt::FileHeader& header) {
        std::cout << "Magic: \t\t\t" << std::string_view(header.magic, 4) << std::endl;
        std::cout << "Version: \t\t" << header.version << std::endl;
        std::cout << "Data type: \t\t" << biomxt::dtype_to_string(header.dtype) << std::endl;
        std::cout << "Compress algorithm: \t" << biomxt::algo_to_string(header.algo) << std::endl;
        std::cout << "Row counts: \t\t" << header.nrow << std::endl;
        std::cout << "Column counts: \t\t" << header.ncolumn << std::endl;
        std::cout << "Chunk size: \t\t" << header.chunk_size << std::endl;
        std::cout << "Chunk table offset: \t" << header.chunk_table_offset << std::endl;
        std::cout << "Names table offset: \t" << header.names_table_offset << std::endl;
    }

    /**
     * @brief Index entry struct. 索引条目结构体.
     */
    struct IndexEntry {
        /**
         * @brief Offset. 偏移量.
         */
        uint64_t offset;

        /**
         * @brief Size. 大小.
         */
        uint32_t size;

        /**
         * @brief Padding. 填充.
         */
        uint32_t padding = 0;
    };

}

#pragma pack(pop)