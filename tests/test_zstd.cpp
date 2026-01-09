#include <iostream>
#include <vector>
#include <random>
#include <numeric>
#include <zstd.h>
#include <cstdint>

int main() {
    const size_t num_elements = 50000;
    const float sparsity = 0.8f;
    const int rounds = 100;

    // 1. 准备原始数据
    std::vector<float> data(num_elements);
    std::default_random_engine generator;
    std::uniform_real_distribution<float> sparsity_dist(0.0, 1.0);
    std::uniform_real_distribution<float> value_dist(0.1, 100.0);

    for (size_t i = 0; i < num_elements; ++i) {
        if (sparsity_dist(generator) < sparsity) {
            data[i] = 0.0f;
        } else {
            data[i] = value_dist(generator);
        }
    }

    size_t src_size = data.size() * sizeof(float);
    std::cout << "Original Size: " << src_size / 1024.0 << " KB" << std::endl;

    // 2. Zstd 压缩测试
    size_t const max_dst_size = ZSTD_compressBound(src_size);
    std::vector<char> compressed_buffer(max_dst_size);

    size_t total_compressed_size = 0;

    std::cout << "Running " << rounds << " rounds of compression..." << std::endl;
    
    for (int i = 0; i < rounds; ++i) {
        // 使用压缩级别 3 (Zstd 默认建议)
        size_t c_size = ZSTD_compress(compressed_buffer.data(), max_dst_size, 
                                      data.data(), src_size, 3);
        
        if (ZSTD_isError(c_size)) {
            std::cerr << "Zstd Error: " << ZSTD_getErrorName(c_size) << std::endl;
            return 1;
        }
        total_compressed_size += c_size;
    }

    // 3. 统计结果
    double avg_compressed_size = (double)total_compressed_size / rounds;
    double compression_ratio = (double)src_size / avg_compressed_size;

    std::cout << "--- Results ---" << std::endl;
    std::cout << "Average Compressed Size: " << avg_compressed_size / 1024.0 << " KB" << std::endl;
    std::cout << "Compression Ratio: " << compression_ratio << "x" << std::endl;
    std::cout << "Space Saving: " << (1.0 - (avg_compressed_size / src_size)) * 100.0 << "%" << std::endl;

    return 0;
}