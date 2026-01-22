#pragma once
#include <cstdint>
#include "biomxt/internel/cache/block_key.hpp"


namespace biomxt {
    class CacheEntry {
        private:
        BlockKey _key;
        std::vector<char> _data;

        public:
        /**
         * @brief Construct a new Cache Entry object.
         * 
         * @param key The block key.
         * @param data The block data.
         * @note The data param used as right-value, will be moved into the cache entry.
         */
        CacheEntry(BlockKey key, std::vector<char>&& data)
            : _key(key), _data(std::move(data)) {}

        /**
         * @brief Get the data of the cache entry.
         * 
         * @return const std::vector<char>& The block data.
         */
        const std::vector<char>& data() const {
            return _data;
        }

        /**
         * @brief Get the key of the cache entry.
         * 
         * @return const BlockKey& The block key.
         */
        const BlockKey& key() const {
            return _key;
        }

        /**
         * @brief Get the size of the cache entry.
         * 
         * @return size_t The size of the cache entry in bytes.
         */
        size_t size() const {
            return sizeof(BlockKey) + _data.capacity();
        }

        bool operator==(const CacheEntry& other) const {
            return _key == other._key && _data == other._data;
        }
    };
}