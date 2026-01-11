#include <iostream>
#include <vector>
#include <random>
#include <numeric>
#include <zstd.h>
#include <cstdint>
#include <cstring>

// Shuffle 函数：将 float 数组的字节重新排列
// 假设输入是 [f1, f2, f3, f4]
// 每个 f 由 4 个字节组成: [b0, b1, b2, b3]
// Shuffle 后: [所有数字的 b0, 所有数字的 b1, 所有数字的 b2, 所有数字的 b3]
void shuffle_floats(const float* src, uint8_t* dest, size_t count) {
    const uint8_t* src_bytes = reinterpret_cast<const uint8_t*>(src);
    for (size_t i = 0; i < count; ++i) {
        dest[i]             = src_bytes[i * 4 + 0];
        dest[i + count]     = src_bytes[i * 4 + 1];
        dest[i + count * 2] = src_bytes[i * 4 + 2];
        dest[i + count * 3] = src_bytes[i * 4 + 3];
    }
}

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
    size_t const max_dst_size = ZSTD_compressBound(src_size);
    std::cout << "Original Size: " << src_size / 1024.0 << " KB\n" << std::endl;

    // 2. 准备 Shuffle 后的数据
    std::vector<uint8_t> shuffled_data(src_size);
    shuffle_floats(data.data(), shuffled_data.data(), num_elements);

    // 3. 测试函数：用于重复执行压缩过程
    auto run_test = [&](const void* input_ptr, const std::string& label) {
        std::vector<char> compressed_buffer(max_dst_size);
        size_t total_size = 0;

        for (int i = 0; i < rounds; ++i) {
            size_t c_size = ZSTD_compress(compressed_buffer.data(), max_dst_size, 
                                          input_ptr, src_size, 3);
            if (ZSTD_isError(c_size)) return (size_t)0;
            total_size += c_size;
        }

        double avg = (double)total_size / rounds;
        std::cout << "[" << label << "]" << std::endl;
        std::cout << "  Avg Compressed: " << avg / 1024.0 << " KB" << std::endl;
        std::cout << "  Ratio: " << (double)src_size / avg << "x" << std::endl;
        std::cout << "  Savings: " << (1.0 - (avg / src_size)) * 100.0 << "%\n" << std::endl;
        return (size_t)avg;
    };

    // 运行对比测试
    run_test(data.data(), "Normal Zstd");
    run_test(shuffled_data.data(), "Shuffle + Zstd");

    return 0;
}