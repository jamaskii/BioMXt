#include <string>
#include <vector>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <chrono>
#include <span>
#include "zstd.h"
#include "cli_app.hpp"
#include "biomxt/biomxt_file.hpp"
#include "biomxt/biomxt_converter.hpp"


namespace fs = std::filesystem;


uint64_t get_timestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}


// 核心转换逻辑封装
bool convert_csv_bmxt(std::string input, std::string output, uint32_t block_width, uint32_t block_height, char sep, const biomxt::DataType dtype, biomxt::CompressAlgorithm algo)
{
    // Print params
    std::cout << "---- Conversion Parameters ----" << std::endl;
    std::cout << "Input: " << input << std::endl;
    std::cout << "Output: " << output << std::endl;
    std::cout << "Block width: " << block_width << std::endl;
    std::cout << "Block height: " << block_height << std::endl;
    std::cout << "Separator: " << sep << std::endl;
    std::cout << "Data type: " << biomxt::dtype_to_string(dtype) << std::endl;
    std::cout << "Compression algo: " << biomxt::algo_to_string(algo) << std::endl;
    std::cout << "-------------------------------" << std::endl;
    std::cout << "Converting..." << std::endl;

    // Convert
    std::vector<std::string> warnings;
    biomxt::FileHeader header;
    switch (dtype) {
        case biomxt::DataType::INT16:
            header = biomxt::csv_to_bmxt<int16_t>(input, output, block_width, block_height, sep, algo, warnings);
            break;
        case biomxt::DataType::INT32:
            header = biomxt::csv_to_bmxt<int32_t>(input, output, block_width, block_height, sep, algo, warnings);
            break;
        case biomxt::DataType::INT64:
            header = biomxt::csv_to_bmxt<int64_t>(input, output, block_width, block_height, sep, algo, warnings);
            break;
        case biomxt::DataType::FLOAT32:
            header = biomxt::csv_to_bmxt<float>(input, output, block_width, block_height, sep, algo, warnings);
            break;
        case biomxt::DataType::FLOAT64:
            header = biomxt::csv_to_bmxt<double>(input, output, block_width, block_height, sep, algo, warnings);
            break;
        default:
            throw std::runtime_error("biomxt::csv_to_bmxt: Invalid data type.");
    }
    for (const std::string &warn : warnings)
    {
        std::cerr << "Warning: " << warn << std::endl;
    }

    std::cout << "Row count: " << header.nrow << std::endl;
    std::cout << "Col count: " << header.ncol << std::endl;
    std::cout << "Block count: " << header.block_count << std::endl;

    std::cout << "Conversion completed successfully." << std::endl;
    return true;
}

int main(int argc, char *argv[])
{
    // Build CLI app
    cliapp::Command bmxt = cliapp::Command("bmxt", "\tConvert CSV/TSV to BioMXt format")
        .add_argument(cliapp::Argument("input", "Input file path"))
        .add_option(cliapp::Option::option_with_value("--output", "-o", "Output file path", ""))
        .add_option(cliapp::Option::option_with_value("--block-width", "-w", "Block width, default: 512", "512"))
        .add_option(cliapp::Option::option_with_value("--block-height", "-h", "Block height, default: 512", "512"))
        .add_option(cliapp::Option::option_with_value("--algorithm", "-a", "Compression algorithm: zstd(default), gzip, lz4", "zstd"))
        .add_option(cliapp::Option::option_with_value("--separator", "-s", "Separator: ',' or '\\t'. Detect by file extension if not specified, and comma as default if detect failed.", ","))
        .add_option(cliapp::Option::option_with_value("--data-type", "-t", "Data type: int16, int32, int64, float32(default), float64", "float32"))
        .add_option(cliapp::Option::option_without_value("--overwrite", "-f", "Overwrite output file if exists"));

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
            if (!bmxt.find_option("--overwrite", "-f").is_provided()) {
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
            dtype = biomxt::dtype_from_string(dtype_opt.get_value());
        }

        // Confirm compression algorithm
        biomxt::CompressAlgorithm algo = biomxt::CompressAlgorithm::ZSTD;
        cliapp::Option algo_opt = bmxt.find_option("--algorithm", "-a");
        if (algo_opt.is_provided()) {
            algo = biomxt::algo_from_string(algo_opt.get_value());
        }
        
        // Confirm block width and height
        uint32_t block_width = 512;
        cliapp::Option block_width_opt = bmxt.find_option("--block-width", "-w");
        if (block_width_opt.is_provided()) {
            block_width = std::stoul(block_width_opt.get_value());
        }
        uint32_t block_height = 512;
        cliapp::Option block_height_opt = bmxt.find_option("--block-height", "-h");
        if (block_height_opt.is_provided()) {
            block_height = std::stoul(block_height_opt.get_value());
        }
        
        // Run conversion
        return convert_csv_bmxt(input.get_value(), output, block_width, block_height, sep, dtype, algo) ? 0 : 1;

    // }
    } else if (header.is_provided()) {
        cliapp::Argument input = header.find_argument("input");

        // Open input file
        try {
            biomxt::BiomxtFile bmxt = biomxt::BiomxtFile(input.get_value());
            biomxt::FileHeader header = bmxt.get_header();
            biomxt::print_bmxt_header(header);

            // std::vector<std::string> rownames = bmxt.get_row_names({0, 1, 2, 3, 4, header.nrow-5, header.nrow-4, header.nrow-3, header.nrow-2, header.nrow-1});
            // for (std::string& rowname : rownames) {
            //     std::cout << rowname << std::endl;
            // }

            // std::vector<char> buffer(header.block_width * header.block_height);
            // bmxt.read_column_data("AP1_AGAGAATCAGGTCAAG.1", buffer);
            // biomxt::Cells cells(buffer);
            
            
            

            // Print first 10 cells
            // for (size_t i=0; i<10; i++) {
            //     std::cout << "Cell[" << i << "] = " << cells[i] << std::endl;
            // }

            // uint64_t start_time = get_timestamp();
            // std::cout << "模拟22951次读取" << start_time << std::endl;
            // std::vector<float> cells;
            // size_t cur_chunk = 0;
            // for (size_t i=0; i<22951; i++) {
            //     if (cur_chunk > 6880) {
            //         cur_chunk = 0;
            //     }
            //     bmxt.read_chunk(cur_chunk, cells);
            //     cur_chunk++;
            // }
            // bmxt.read_chunk(0, cells);
            // std::cout << "模拟22951次读取耗时" << get_timestamp() - start_time << std::endl;
            
            bmxt.read_row(0, [](auto cells) {
                std::cout << "Cell index : 0" << std::endl;
                std::cout << "Cell count: " << cells.size() << std::endl;
                // Print first 10 cells
                for (size_t i=0; i<10; i++) {
                    std::cout << "Cell[" << i << "] = " << cells[i] << std::endl;
                }
            });
            
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