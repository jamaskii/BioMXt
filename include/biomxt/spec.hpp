#pragma once
#include <cstdint>

#pragma pack(push, 1)

namespace biomxt {

    enum DataType : uint8_t {
        INT16 = 0,
        INT32 = 1,
        INT64 = 2,
        FLOAT32 = 3,
        FLOAT64 = 4
    };

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

    inline DataType string_to_dtype(const std::string& type) {
        if (type == "int16") return DataType::INT16;
        if (type == "int32") return DataType::INT32;
        if (type == "int64") return DataType::INT64;
        if (type == "float64") return DataType::FLOAT64;
        return DataType::FLOAT32;
    }

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
        uint32_t ncol;
        uint32_t chunk_size;
        uint8_t padding1[4] = {0};
        uint64_t chunk_table_offset;
        uint64_t names_table_offset;
        uint8_t padding2[24] = {0};
    };

    struct IndexEntry {
        uint64_t offset;
        uint32_t size;
        uint32_t padding = 0;
    };

}

#pragma pack(pop)