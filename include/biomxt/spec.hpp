#pragma once
#include <cstdint>

#pragma pack(push, 1)

enum BMxtDataType : uint8_t {
    INT16 = 0,
    INT32 = 1,
    INT64 = 2,
    FLOAT32 = 3,
    FLOAT64 = 4
};

struct BMxtHeader {
    char magic[4] = {'B', 'M', 'X', 't'};
    uint16_t version = 1;
    uint8_t dtype = BMxtDataType::FLOAT32;
    uint8_t reserved = 0;
    uint32_t rows;
    uint32_t cols;
    uint32_t chunk_size;
    uint64_t idx_offset;
    uint64_t name_offset;
    uint64_t data_offset;
    uint8_t padding[20] = {0};
};

struct BMxtChunkIndex {
    uint64_t offset;
    uint32_t size;
};

#pragma pack(pop)