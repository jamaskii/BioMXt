#pragma once
#include <cstdint>
#include <iostream>


namespace biomxt
{
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
         * @brief Raw/uncompresse size.
         */
        uint32_t raw_size;
    };
} // namespace biomxt
