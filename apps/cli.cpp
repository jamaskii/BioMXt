#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include "biomxt/spec.hpp"
#include "biomxt/csv_parser.hpp"
#include "zstd.h"
#include "biomxt/biomxt.hpp"

namespace fs = std::filesystem;

// 打印帮助信息
void print_usage() {
    std::cout << "BioMXt CLI Converter\n"
              << "Usage: biomxt convert --input <file> [options]\n\n"
              << "Options:\n"
              << "  --input        Input CSV file path (Required)\n"
              << "  --output       Output .bmxt file path (Optional)\n"
              << "  --first-col-as-row  Treat first column as rownames\n"
              << "  --chunk-size   Number of elements per chunk (Default: 50000)\n"
              << "  --separation   CSV separator character (Default: ',')\n"
              << "  --dtype        Data type: int16, int32, int64, float32, float64 (Default: float32)\n"
              << "  --algo         Compression: zstd, gzip, lz4 (Default: zstd)\n"
              << std::endl;
}

// 核心转换逻辑封装
int run_convert(int argc, char* argv[]) {
    // Parse arguments
    std::string input, output;
    uint32_t chunk_size = 50000;
    char sep = ',';
    bool user_specified_sep = false;
    biomxt::DataType dtype = biomxt::DataType::FLOAT32;
    biomxt::CompressAlgo algo = biomxt::CompressAlgo::ZSTD; 
    bool first_column_as_rownames = false;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) input = argv[++i];
        else if (arg == "--output" && i + 1 < argc) output = argv[++i];
        else if (arg == "--chunk-size" && i + 1 < argc) chunk_size = std::stoul(argv[++i]);
        else if (arg == "--separation" && i + 1 < argc) {
            user_specified_sep = true;
            if (argv[++i] == "\\t") sep = '\t';
            else if (argv[++i] == ",") sep = ',';
            else {
                std::cerr << "Error: unknown separation character [" << argv[++i] << "]." << std::endl;
                return 1;
            }
        }
        else if (arg == "--dtype" && i + 1 < argc) dtype = biomxt::string_to_dtype(argv[++i]);
        else if (arg == "--algo" && i + 1 < argc) algo = biomxt::string_to_algo(argv[++i]);
        else if (arg == "--first-column-as-rownames") first_column_as_rownames = true;
        else {
            std::cerr << "Warning: unknown option [" << arg << "]." << std::endl;
        }
    }

    // Check arguments
    if (input.empty()) {
        std::cerr << "Error: --input is required." << std::endl;
        print_usage();
        return 1;
    }

    // Check input file exists
    if (!fs::exists(input)) {
        std::cerr << "Error: input file [" << input << "] does not exist." << std::endl;
        return 1;
    }

    // Output file set to default if not specified
    if (output.empty()) {
        output = fs::path(input).replace_extension(".bmxt").string();
    }

    // Create output dir if not exists
    if (!fs::exists(fs::path(output).parent_path())) {
        if(!fs::create_directories(fs::path(output).parent_path())) {
            std::cerr << "Error: failed to create output dir [" << fs::path(output).parent_path().string() << "]." << std::endl;
            return 1;
        }
    }

    // Confirm separator by file extension if not specified
    if (!user_specified_sep) {
        std::string ext = fs::path(input).extension().string();
        if (ext == ".tsv") sep = '\t';
        else if (ext == ".csv") sep = ',';
    }

    // Check compression algo
    if (algo != biomxt::CompressAlgo::ZSTD) {
        std::cerr << "Error: only zstd compression is supported currently." << std::endl;
        return 1;
    }

    std::cout << "---- BioMXt converter ----" << std::endl;
    std::cout << "Input: " << input << std::endl;
    std::cout << "Output: " << output << std::endl;
    std::cout << "Chunk size: " << chunk_size << std::endl;
    std::cout << "Separator: " << sep << std::endl;
    std::cout << "Data type: " << biomxt::dtype_to_string(dtype) << std::endl;
    std::cout << "Compression algo: " << biomxt::algo_to_string(algo) << std::endl;
    std::cout << "First column as rownames: " << (first_column_as_rownames ? "Yes" : "No") << std::endl;
    std::cout << "Converting..." << std::endl;

    // Convert
    std::string error;
    std::vector<std::string> warnings;
    biomxt::FileHeader header = biomxt::csv_to_bmxt(input, output, first_column_as_rownames, chunk_size, sep, dtype, algo, error, warnings);
    if (!error.empty()) {
        std::cerr << "Error: " << error << std::endl;
        return 1;
    }
    for (const std::string& warn : warnings) {
        std::cerr << "Warning: " << warn << std::endl;
    }
    
    std::cout << "Row count: " << header.nrow << std::endl;
    std::cout << "Col count: " << header.ncol << std::endl;

    std::cout << "Conversion completed successfully." << std::endl;
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    std::string cmd = argv[1];
    if (cmd == "convert") {
        return run_convert(argc, argv);
    } else {
        std::cerr << "Unknown command: " << cmd << std::endl;
        print_usage();
        return 1;
    }
}