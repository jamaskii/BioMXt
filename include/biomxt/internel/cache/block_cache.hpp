#pragma once
#include <cstdint>
#include <vector>
#include <shared_mutex>
#include <mutex>
#include <list>
#include <unordered_map>
#include <iostream>
#include "biomxt/internel/cache/cache_entry.hpp"


namespace biomxt {
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