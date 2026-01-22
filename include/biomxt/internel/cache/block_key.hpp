#pragma once
#include <cstdint>
#include <vector>
#include "biomxt/internel/struct/uuid.hpp"


namespace biomxt {
    class BlockKey {
        private:
        uint32_t _block_index;
        biomxt::UUID _uuid;

        public:
        /**
         * @brief Construct a new Block Key object.
         * 
         * @param block_index The block index.
         * @param uuid The block UUID.
         */
        BlockKey(uint32_t block_index, const biomxt::UUID& uuid)
            : _block_index(block_index), _uuid(uuid) {}

        bool operator==(const BlockKey& other) const {
            return _block_index == other._block_index && _uuid == other._uuid;
        }

        uint32_t block_index() const {
            return _block_index;
        }

        const biomxt::UUID& uuid() const {
            return _uuid;
        }
    };

    struct BlockKeyHash {
        std::size_t operator()(const BlockKey& k) const {
            // Fetch the high and low 64 bits of the UUID
            uint64_t h1, h2;
            std::memcpy(&h1, k.uuid().data, 8);
            std::memcpy(&h2, k.uuid().data + 8, 8);

            // Use the classic Hash Combine algorithm to combine the UUID and block_index
            // This ensures that any change in the UUID bits will result in a significant change in the hash value
            std::size_t seed = std::hash<uint64_t>{}(h1);
            seed ^= std::hash<uint64_t>{}(h2) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<uint32_t>{}(k.block_index()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            
            return seed;
        }
    };

}