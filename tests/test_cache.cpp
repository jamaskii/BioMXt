#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include "biomxt/biomxt_file.hpp"
#include "biomxt/cache.hpp"
#include "biomxt/spec.hpp"
#include "zstd.h"


#define BMXT_FILE                   "test_data/PRJNA978570_RNA_data.bmxt"
#define MEMORY_LIMITS               {0, 1024 * 1024 * 2, 1024 * 1024 * 8, 1024 * 1024 * 32, 1024 * 1024 * 64, 1024 * 1024 * 128, 1024 * 1024 * 256, 1024 * 1024 * 512, (uint64_t)1024 * 1024 * 1024, (uint64_t)1024 * 1024 * 2048}
#define BLOCK_COUNT_PER_TEST        10000
#define MISS_DENOMINATORS           {2, 4, 8, 9}

uint64_t get_timestamp() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

int main() {
    std::vector<uint32_t> miss_denominators = MISS_DENOMINATORS;
    std::vector<uint64_t> memory_limits = MEMORY_LIMITS;
    std::cout << "Per Test will read " << BLOCK_COUNT_PER_TEST << " blocks under different memory limits and miss rate." << std::endl;
    std::cout << "Target file: " << BMXT_FILE << std::endl;
    
    biomxt::BlockCache block_cache;
    biomxt::BiomxtFile file = biomxt::BiomxtFile(BMXT_FILE, &block_cache);
    std::vector<char> cells;

    // Test different miss denominator with cache.
    for (uint32_t memory_limit : memory_limits) {
        block_cache.set_memory_limit(memory_limit);
        std::cout << "Set cache memory limit: " << block_cache.get_memory_limit() / 1024.0 / 1024.0 << " MB" << std::endl;
        for (uint32_t miss_denominator : miss_denominators) {
            uint64_t total_time = 0;
            uint64_t start_time = 0;
            uint32_t target_block = 0;
            size_t tested_block = 0;
            
            while (tested_block < BLOCK_COUNT_PER_TEST) {
                start_time = get_timestamp();
                if (tested_block % miss_denominator == 1) {
                    target_block = rand() % file.get_header().block_count;
                }
                file.read_block(target_block, cells);
                total_time += get_timestamp() - start_time;
                tested_block++;
            }
            std::cout << "Average read block cost: " << static_cast<double>(total_time) / BLOCK_COUNT_PER_TEST / 1000.0<< " ms, under miss rate: " << 1.0 / miss_denominator << std::endl;
        }
    }

    block_cache.set_memory_limit(file.get_header().block_count * file.get_max_uncompressed_block_size());
    std::cout << "Set cache memory limit: " << block_cache.get_memory_limit() / 1024.0 / 1024.0 << " MB (recommend maximal)" << std::endl;
    for (uint32_t miss_denominator : miss_denominators) {
        uint64_t total_time = 0;
        uint64_t start_time = 0;
        uint32_t target_block = 0;
        size_t tested_block = 0;
        
        while (tested_block < BLOCK_COUNT_PER_TEST) {
            start_time = get_timestamp();
            if (tested_block % miss_denominator == 1) {
                target_block = rand() % file.get_header().block_count;
            }
            file.read_block(target_block, cells);
            total_time += get_timestamp() - start_time;
            tested_block++;
        }
        std::cout << "Average read block cost: " << static_cast<double>(total_time) / BLOCK_COUNT_PER_TEST / 1000.0<< " ms, under miss rate: " << 1.0 / miss_denominator << std::endl;
    }

    file.close();
    file = biomxt::BiomxtFile(BMXT_FILE);
    std::cout << "Set cache memory limit: " << file.get_block_cache_memory_limit() / 1024.0 / 1024.0 << " MB (recommend minimal)" << std::endl;
    for (uint32_t miss_denominator : miss_denominators) {
        uint64_t total_time = 0;
        uint64_t start_time = 0;
        uint32_t target_block = 0;
        size_t tested_block = 0;
        
        while (tested_block < BLOCK_COUNT_PER_TEST) {
            start_time = get_timestamp();
            if (tested_block % miss_denominator == 1) {
                target_block = rand() % file.get_header().block_count;
            }
            file.read_block(target_block, cells);
            total_time += get_timestamp() - start_time;
            tested_block++;
        }
        std::cout << "Average read block cost: " << static_cast<double>(total_time) / BLOCK_COUNT_PER_TEST / 1000.0<< " ms, under miss rate: " << 1.0 / miss_denominator << std::endl;
    }

    return 0;
}