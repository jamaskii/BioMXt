#pragma once
#include <cstdint>

#pragma pack(push, 1)

namespace biomxt {

    // ---- Data type ----
    // Enum
    enum DataType : uint8_t {
        UNKNOWN = 0,
        INT16 = 1,
        INT32 = 2,
        INT64 = 3,
        FLOAT32 = 4,
        FLOAT64 = 5
    };

    // Enum to string
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

    // String to enum
    inline DataType string_to_dtype(const std::string& type) {
        if (type == "int16") return DataType::INT16;
        if (type == "int32") return DataType::INT32;
        if (type == "int64") return DataType::INT64;
        if (type == "float" || type == "float32") return DataType::FLOAT32;
        if (type == "float64") return DataType::FLOAT64;
        return DataType::UNKNOWN;
    }

    // Class to enum
    template<typename T>
               struct dtype_from          { static constexpr DataType value = DataType::UNKNOWN; static constexpr bool valid = false; };
    template<> struct dtype_from<int16_t> { static constexpr DataType value = DataType::INT16; static constexpr bool valid = true; };
    template<> struct dtype_from<int32_t> { static constexpr DataType value = DataType::INT32; static constexpr bool valid = true; };
    template<> struct dtype_from<int64_t> { static constexpr DataType value = DataType::INT64; static constexpr bool valid = true; };
    template<> struct dtype_from<float>   { static constexpr DataType value = DataType::FLOAT32; static constexpr bool valid = true; };
    template<> struct dtype_from<double>  { static constexpr DataType value = DataType::FLOAT64; static constexpr bool valid = true; };

    enum CompressAlgo : uint8_t {
        ZSTD = 0,
        GZIP = 1,
        LZ4 = 2
    };

    inline std::string algo_to_string(CompressAlgo algo) {
        switch (algo) {
            case ZSTD: return "zstd";
            case GZIP: return "gzip";
            case LZ4: return "lz4";
            default: return "unknown";
        }
    }

    inline CompressAlgo string_to_algo(const std::string& algo) {
        if (algo == "zstd") return CompressAlgo::ZSTD;
        if (algo == "gzip") return CompressAlgo::GZIP;
        if (algo == "lz4") return CompressAlgo::LZ4;
        return CompressAlgo::ZSTD;
    }

    struct FileHeader {
        char magic[4] = {'B', 'M', 'X', 't'};
        uint16_t version = 1;
        DataType dtype = DataType::FLOAT32;
        CompressAlgo algo = CompressAlgo::ZSTD;
        uint32_t nrow;
        uint32_t ncolumn;
        uint32_t chunk_size;
        uint8_t padding1[4] = {0};
        uint64_t chunk_table_offset;
        uint64_t names_table_offset;
        uint8_t padding2[24] = {0};
    };

    void print_bmxt_header(const biomxt::FileHeader& header) {
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

    struct IndexEntry {
        uint64_t offset;
        uint32_t size;
        uint32_t padding = 0;
    };

}

#pragma pack(pop)