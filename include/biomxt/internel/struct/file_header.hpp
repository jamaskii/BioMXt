#pragma once
#include <cstdint>
#include <iostream>
#include "biomxt/internel/struct/data_type.hpp"
#include "biomxt/internel/struct/uuid.hpp"
#include "biomxt/internel/struct/compress_algorithm.hpp"


#pragma pack(push, 1)

namespace biomxt {
    /**
     * @brief File header struct.
     */
    struct FileHeader {
        char magic[4] = {'B', 'M', 'X', 't'};

        uint16_t version = 1;

        DataType dtype = DataType::FLOAT32;

        CompressAlgorithm algo = CompressAlgorithm::ZSTD;

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
}

#pragma pack(pop)