#pragma once
#include <cstdint>
#include <iostream>


namespace biomxt
{
    /**
     * @brief Compress algorithm enum.
     */
    enum CompressAlgorithm : uint8_t {
        ZSTD = 0,
        GZIP = 1,
        LZ4 = 2
    };

    /**
     * @brief Convert compress algorithm enum to string.
     * @param algo Compress algorithm enum.
     * @return std::string String representation of compress algorithm.
     */
    inline std::string algo_to_string(CompressAlgorithm algo) {
        switch (algo) {
            case ZSTD: return "zstd";
            case GZIP: return "gzip";
            case LZ4: return "lz4";
            default: return "unknown";
        }
    }

    /**
     * @brief Convert string to compress algorithm enum.
     * @param algo String representation of compress algorithm.
     * @return `biomxt::CompressAlgorithm` Compress algorithm enum.
     */
    inline CompressAlgorithm algo_from_string(const std::string& algo) {
        if (algo == "zstd") return CompressAlgorithm::ZSTD;
        if (algo == "gzip") return CompressAlgorithm::GZIP;
        if (algo == "lz4") return CompressAlgorithm::LZ4;
        return CompressAlgorithm::ZSTD;
    }
} // namespace biomxt

