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

    enum CompressAlgo : uint8_t {
        ZSTD = 0,
        GZIP = 1,
        LZ4 = 2
    };

    struct FileHeader {
        char magic[4] = {'B', 'M', 'X', 't'};
        uint16_t version = 1;
        uint8_t dtype = DataType::FLOAT32;
        uint8_t algo = CompressAlgo::ZSTD;
        uint32_t nrow;
        uint32_t ncol;
        uint32_t chunk_size;
        uint64_t chunk_table_offset;
        uint64_t names_table_offset;
        uint8_t padding[36] = {0};
    };

    struct IndexEntry {
        uint64_t offset;
        uint32_t size;
        uint32_t padding = 0;
    };

}

#pragma pack(pop)