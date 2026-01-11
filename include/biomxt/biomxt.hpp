#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include "biomxt/csv_parser.hpp"
#include "biomxt/spec.hpp"
#include "zstd.h"


namespace biomxt {

    bool chunk_flush(
        std::vector<float>& chunk, 
        std::ofstream& out, 
        std::vector<biomxt::IndexEntry>& chunk_indices, 
        biomxt::CompressAlgo algo);

    biomxt::FileHeader csv_to_bmxt(
        const std::string& input_file, 
        const std::string& output_file, 
        bool first_col_as_rownames, 
        uint64_t chunk_size, 
        char separation, 
        biomxt::DataType dtype, 
        biomxt::CompressAlgo algo, 
        std::string& error, 
        std::vector<std::string>& warnings);
        
}