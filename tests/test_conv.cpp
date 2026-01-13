#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include "biomxt/csv_parser.hpp"
#include "biomxt/spec.hpp"
#include "biomxt/biomxt.hpp"
#include "zstd.h"


#define ARG_INPUT_FILE              "test_data/PRJNA978570_RNA_data.csv"
#define ARG_OUTPUT_FILE             "test_data/PRJNA978570_RNA_data.bmxt"
#define ARG_FIRST_COL_AS_ROWNAMES   true
#define ARG_CHUNK_SIZE              50000
#define ARG_SEPARATION_CHAR         ','
#define ARG_DTYPE                   biomxt::DataType::FLOAT32
#define ARG_ALGO                    biomxt::CompressAlgo::ZSTD
#define TEST_EPOCHES                3


uint64_t get_timestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

biomxt::FileHeader run_test() {
    std::string error;
    std::vector<std::string> warnings;
    biomxt::FileHeader header = biomxt::csv_to_bmxt(ARG_INPUT_FILE, ARG_OUTPUT_FILE, ARG_FIRST_COL_AS_ROWNAMES, ARG_CHUNK_SIZE, ARG_SEPARATION_CHAR, ARG_DTYPE, ARG_ALGO, error, warnings);
    if (!error.empty()) {
        std::cerr << "Error: " << error << std::endl;
        return header;
    }
    for (const std::string& warn : warnings) {
        std::cerr << "Warning: " << warn << std::endl;
    }
    return header;
}

int main() {
    biomxt::FileHeader header;
    size_t cur_epoch = 0;
    uint64_t total_time = 0;
    while (cur_epoch < TEST_EPOCHES) {
        uint64_t start_time = get_timestamp();
        header = run_test();
        uint64_t cost_time = get_timestamp() - start_time;
        total_time += cost_time;
        std::cout << "Epoch " << cur_epoch + 1 << " cost time: " << static_cast<double>(cost_time) / 1000.0 << " s" << std::endl;
        cur_epoch++;
    }
    std::cout << "Average cost time: " << static_cast<double>(total_time) / TEST_EPOCHES / 1000.0 << " s" << std::endl;
    print_bmxt_header(header);
    return 0;
}