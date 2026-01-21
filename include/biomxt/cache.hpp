#pragma once
#include <cstdint>
#include <vector>
#include <shared_mutex>
#include <list>
#include <unordered_map>
#include <iostream>
#include "biomxt/uuid.hpp"


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
            // 分别取出 UUID 的高 64 位和低 64 位
            uint64_t h1, h2;
            std::memcpy(&h1, k.uuid().data, 8);
            std::memcpy(&h2, k.uuid().data + 8, 8);

            // 使用经典的 Hash Combine 算法将 UUID 和 block_index 融合
            // 这样可以确保 UUID 的每一位变化都会导致哈希值大幅变动
            std::size_t seed = std::hash<uint64_t>{}(h1);
            seed ^= std::hash<uint64_t>{}(h2) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= std::hash<uint32_t>{}(k.block_index()) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            
            return seed;
        }
    };

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

    class BlockCache {
        private:
            mutable std::shared_mutex _mutex;
            
            // LRU cache list and map
            std::list<CacheEntry> _block_cache_list;
            // Map between block key <-> list iterator
            std::unordered_map<BlockKey, std::list<CacheEntry>::iterator, BlockKeyHash> _map;

            // RAM used counts
            size_t _memory_used = 0;

            // Max RAM limit, default 128MB
            size_t _memory_limit = 1024 * 1024 * 128;

        public:
            BlockCache() = default;
            ~BlockCache() = default;
            /**
             * @brief Get the memory limit of the cache.
             * 
             * @return size_t The memory limit in bytes.
             */
            size_t get_memory_limit() const {
                std::shared_lock lock(_mutex);
                return _memory_limit;
            }

            /**
             * @brief Set the memory limit of the cache.
             * 
             * @param bytes The memory limit in bytes.
             * @note The cache will evict entries immediately after setting new limit.
             */
            void set_memory_limit(size_t bytes) {
                std::unique_lock lock(_mutex);
                _memory_limit = bytes;
                // Evict entries immediately after setting new limit
                _evict_until_fit();
            }

            /**
             * @brief Get the memory used by the cache.
             * 
             * @return size_t The memory used by the cache in bytes.
             */
            size_t get_memory_used() const {
                std::shared_lock lock(_mutex);
                return _memory_used;
            }

            /**
             * @brief Insert a block into the cache.
             * 
             * @param key The key of the block.
             * @param data The compressed data of the block.
             * @note The data used as right-value, will be moved into the cache.
             */
            void insert(const BlockKey& key, std::vector<char>&& data) {
                std::unique_lock lock(_mutex);

                // Ignore if data size exceeds max limit
                if (data.size() > _memory_limit) return;

                // Remove old entry if exists
                auto it = _map.find(key);
                if (it != _map.end()) {
                    // Update memory used
                    _memory_used -= it->second->size();    
                    // Remove old entry from map and list
                    _map.erase(it);
                    _block_cache_list.erase(it->second);
                }

                // Evict until enough space
                _evict_until_enough(data.size());

                // Insert entry to cache list, update counter and map.
                _block_cache_list.emplace_front(key, std::move(data));
                _memory_used += _block_cache_list.begin()->size();
                _map[key] = _block_cache_list.begin();
            }

            /**
             * @brief Get the block data from the cache.
             * 
             * @param key The key of the block.
             * @return const std::vector<char>& The block data.
             */
            bool get_block_data(const BlockKey& key, std::vector<char>& buffer, size_t offset, size_t size) {
                std::unique_lock lock(_mutex);
                // Find entry by key
                auto it = _map.find(key);
                if (it == _map.end()) return false;

                // Move entry to front (most recently used)
                _block_cache_list.splice(_block_cache_list.begin(), _block_cache_list, it->second);
                // Check range 
                if (offset + size > it->second->data().size()) return false;
                // Check buffer size
                if (buffer.size() < size) buffer.resize(size);
                // Copy data
                std::memcpy(buffer.data(), it->second->data().data() + offset, size);
                return true;
            }

        private:
            /**
             * @brief Evict the least recently used block from the cache.
             */
            void _evict_one_least_recent() {
                if (_block_cache_list.empty()) return;
                
                const auto& last = _block_cache_list.back();
                // Update memory used
                _memory_used -= last.size();
                // Delete entry from map and list
                _map.erase(last.key());
                _block_cache_list.pop_back();
            }

            /**
             * @brief Evict blocks from the cache until it fits the memory limit.
             */
            void _evict_until_fit() {
                while (_memory_used > _memory_limit && !_block_cache_list.empty()) {
                    _evict_one_least_recent();
                }
            }

            /**
             * @brief Evict blocks from the cache until it has enough space for the incoming block.
             * 
             * @param incoming_size The size of the incoming block in bytes.
             */
            void _evict_until_enough(size_t incoming_size) {
                while (_memory_used + incoming_size > _memory_limit && !_block_cache_list.empty()) {
                    _evict_one_least_recent();
                }
            }
    };


}