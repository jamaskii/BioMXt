#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include "biomxt/spec.hpp"
#include "biomxt/csv_parser.hpp"
#include "zstd.h"
#include "biomxt/biomxt.hpp"
#include "cli_app.hpp"

namespace fs = std::filesystem;


// 核心转换逻辑封装
bool convert_csv_bmxt(std::string input, std::string output, uint32_t chunk_size, char sep, biomxt::DataType dtype, biomxt::CompressAlgo algo, bool first_column_as_rownames)
{
    // Print params
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
    if (!error.empty())
    {
        std::cerr << "Error: " << error << std::endl;
        return false;
    }
    for (const std::string &warn : warnings)
    {
        std::cerr << "Warning: " << warn << std::endl;
    }

    std::cout << "Row count: " << header.nrow << std::endl;
    std::cout << "Col count: " << header.ncolumn << std::endl;

    std::cout << "Conversion completed successfully." << std::endl;
    return true;
}

int main(int argc, char *argv[])
{
    // Build CLI app
    cliapp::Command bmxt = cliapp::Command("bmxt", "\tConvert CSV/TSV to BioMXt format")
        .add_argument(cliapp::Argument("input", "Input file path"))
        .add_option(cliapp::Option::option_with_value("--output", "-o", "Output file path", ""))
        .add_option(cliapp::Option::option_with_value("--chunk-size", "-c", "Chunk size, default: 50k", "50000"))
        .add_option(cliapp::Option::option_with_value("--algorithm", "-a", "Compression algorithm: zstd(default), gzip, lz4", "zstd"))
        .add_option(cliapp::Option::option_with_value("--separator", "-s", "Separator: ',' or '\\t'. Detect by file extension if not specified, and comma as default if detect failed.", ","))
        .add_option(cliapp::Option::option_with_value("--data-type", "-t", "Data type: int16, int32, int64, float32(default), float64", "float32"))
        .add_option(cliapp::Option::option_without_value("--first-column-as-rownames", "-r", "First column as rownames"))
        .add_option(cliapp::Option::option_without_value("--overwrite", "-w", "Overwrite output file if exists"));

    cliapp::Command dump = cliapp::Command("dump", "\tDump BioMXt file to CSV/TSV format")
        .add_argument(cliapp::Argument("input", "Input file path"))
        .add_option(cliapp::Option::option_with_value("--output", "-o", "Output file path", ""))
        .add_option(cliapp::Option::option_with_value("--separator", "-s", "Separator: ',' or '\\t'. default: comma", ","))
        .add_option(cliapp::Option::option_without_value("--overwrite", "-w", "Overwrite output file if exists"));

    cliapp::Command cells = cliapp::Command("cells", "\tRead cells from BioMXt file")
        .add_argument(cliapp::Argument("input", "Input file path"))
        .add_option(cliapp::Option::option_with_value("--row-ids", "-r", "\t\tRows match by indices to be read, separated by comma, default or empty: all", ""))
        .add_option(cliapp::Option::option_with_value("--row-names", "-R", "\tRows match by indices to be read, separated by comma, default or empty: all", ""))
        .add_option(cliapp::Option::option_with_value("--col-ids", "-c", "\t\tColumns match by indices to be read, separated by comma, default or empty: all", ""))
        .add_option(cliapp::Option::option_with_value("--col-names", "-C", "\tColumns match by indices to be read, separated by comma, default or empty: all", ""))
        .add_option(cliapp::Option::option_without_value("--show-row-ids", "-sri", "\tShow row indices"))
        .add_option(cliapp::Option::option_without_value("--show-row-names", "-srn", "\tShow row names"))
        .add_option(cliapp::Option::option_without_value("--show-column-ids", "-sci", "Show column indices"))
        .add_option(cliapp::Option::option_without_value("--show-column-names", "-scn", "Show column names"));

    cliapp::Command header = cliapp::Command("header", "Read header from BioMXt file")
        .add_argument(cliapp::Argument("input", "Input file path"));

    cliapp::App app = cliapp::App("biomxt", "0.1.0", "Lite maxtrix format for bioinformatics")
        .add_option(cliapp::Option::option_without_value("--help", "-h", "Print help message"))
        .add_option(cliapp::Option::option_without_value("--version", "-v", "Print version"))
        .add_command(&bmxt)
        .add_command(&dump)
        .add_command(&cells)
        .add_command(&header);

    // Check if CLI usage is valid
    if(!app.parse(argc, argv)) return 1;

    // Match convert command first
    if (bmxt.is_provided()) { 
        // Check if input file exists
        cliapp::Argument input = bmxt.find_argument("input");
        if (!input.is_provided()) {
            std::cerr << "Error: Input file path is required." << std::endl;
            return 1;
        }
        if (!std::filesystem::exists(input.get_value())) {
            std::cerr << "Error: Input file [" << input.get_value() << "] does not exist." << std::endl;
            return 1;
        }

        // Check output file
        cliapp::Option output_opt = bmxt.find_option("--output", "-o");
        std::string output = output_opt.get_value();
        if (!output_opt.is_provided()) {
            // Change output file extension to .bmxt if not specified
            output = fs::path(input.get_value()).replace_extension(".bmxt").string();
        }
        if (std::filesystem::exists(output)) {
            if (!bmxt.find_option("--overwrite", "-w").is_provided()) {
                std::cerr << "Error: Output file already exists." << std::endl;
                return 1;
            }
        }
        if (!std::filesystem::exists(fs::path(output).parent_path())) {
            if (!std::filesystem::create_directories(fs::path(output).parent_path())) {
                std::cerr << "Error: Failed to create output directory." << std::endl;
                return 1;
            }
        }

        // Confirm separator
        char sep = ',';
        cliapp::Option sep_opt = bmxt.find_option("--separator", "-s");
        if (sep_opt.is_provided()) {
            if (sep_opt.get_value() == ",") sep = ',';
            else if (sep_opt.get_value() == "\\t") sep = '\t';
            else {
                std::cerr << "Warning: Invalid separator, use comma as default." << std::endl;
                sep = ',';
            }
        } else {
            std::string ext = fs::path(input.get_value()).extension().string();
            if (ext == ".tsv" || ext == ".TSV") {
                sep = '\t';
            }
            else {
                sep = ',';
            }
        }

        // Confirm data type
        biomxt::DataType dtype = biomxt::DataType::FLOAT32;
        cliapp::Option dtype_opt = bmxt.find_option("--data-type", "-t");
        if (dtype_opt.is_provided()) {
            dtype = biomxt::string_to_dtype(dtype_opt.get_value());
        }

        // Confirm compression algorithm
        biomxt::CompressAlgo algo = biomxt::CompressAlgo::ZSTD;
        cliapp::Option algo_opt = bmxt.find_option("--algorithm", "-a");
        if (algo_opt.is_provided()) {
            algo = biomxt::string_to_algo(algo_opt.get_value());
        }

        // Confirm first column as rownames
        bool first_column_as_rownames = bmxt.find_option("--first-column-as-rownames", "-r").is_provided();
        
        // Confirm chunk size
        uint32_t chunk_size = 50000;
        cliapp::Option chunk_size_opt = bmxt.find_option("--chunk-size", "-c");
        if (chunk_size_opt.is_provided()) {
            chunk_size = std::stoul(chunk_size_opt.get_value());
        }
        
        // Run conversion
        return convert_csv_bmxt(input.get_value(), output, chunk_size, sep, dtype, algo, first_column_as_rownames) ? 0 : 1;

    } else if (header.is_provided()) {
        cliapp::Argument input = header.find_argument("input");

        // Open input file
        try {
            biomxt::BiomxtFile bmxt = biomxt::BiomxtFile(input.get_value());
            biomxt::print_bmxt_header(bmxt.get_header());
            bmxt.close();
        } catch (const std::exception& e) {
            std::cerr << "Error: Failed to open input file [" << input.get_value() << "]." << std::endl;
            return 1;
        }
    }

    // No command provided, check if option provided
    if (app.find_option("--help", "-h").is_provided()) {
        app.print_help();
        return 0;
    }

    if (app.find_option("--version", "-v").is_provided()) {
        std::cout << app.version << std::endl;
        return 0;
    }

    return 0;
}