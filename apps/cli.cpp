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

// // 修正后的 App 类
// class App
// {
// public:
//     // 成员变量声明（调整顺序，与初始化列表保持一致，修正 argv 类型）
//     std::string name;
//     std::vector<Command> commands;
//     int argc;
//     char **argv; // 修正：改为 char**，存储命令行参数指针

//     // 构造函数（修正初始化列表，保持与成员声明顺序一致）
//     App(const std::string &app_name,
//         const std::vector<Command> &app_commands,
//         int app_argc, // 调整参数顺序：先 argc 后 argv，更符合习惯
//         char *app_argv[])
//         : name(app_name),
//           commands(app_commands),
//           argc(app_argc),
//           argv(app_argv) // 合法：指针赋值，传递命令行参数数组地址
//     {
//     }

//     // 打印帮助信息（修正：遍历使用 const 引用）
//     void print_help() const
//     { // 补充 const：该函数不修改 App 类的任何成员变量
//         std::cout << "Usage: " << name << " <command> [arguments] [options]\n\n";
//         std::cout << "Commands:\n";

//         // 修正：使用 const Command&，避免修改，提升安全性
//         for (const Command &cmd : commands)
//         {
//             std::cout << "  " << cmd.name << "\n";
//             cmd.print_help(4); // 调整缩进为 4，格式化输出更美观
//         }
//     }

//     bool get_command(Command &cmd) const
//     {
//         if (argc < 2)
//         {
//             return false;
//         }
//         std::string cmd_name = argv[1];
//         for (const Command &c : commands)
//         {
//             if (c.name == cmd_name)
//             {
//                 cmd = c;
//                 return true;
//             }
//         }
//         return false;
//     }
// };

// class Command
// {
// public:
//     std::string name;
//     std::vector<Argument> args;
//     std::vector<Option> options;

//     Command(const std::string &name, const std::vector<Argument> &args, const std::vector<Option> &options) : name(name), args(args), options(options) {}

//     void print_help(const size_t indent = 2) const
//     {
//         std::cout << std::string(indent, ' ') << "Usage: biomxt " << name << " [arguments] [options]\n\n";
//         std::cout << std::string(indent, ' ') << "Arguments:\n";
//         for (const auto &arg : args)
//         {
//             std::cout << std::string(indent, ' ') << "  " << arg.name << "  " << arg.desc << "\n";
//         }
//         std::cout << std::string(indent, ' ') << "Options:\n";
//         for (const auto &opt : options)
//         {
//             std::cout << std::string(indent, ' ') << "  " << opt.name << "  " << opt.desc << "\n";
//         }
//     }
// };

// class Argument
// {
// public:
//     std::string name;
//     std::string value;
//     std::string desc;

//     Argument(const std::string &name, const std::string &desc) : name(name), desc(desc) {}
// };

// class Option
// {
// public:
//     std::string name;
//     char short_name;
//     std::string value;
//     std::string desc;

//     Option(const std::string &name, char short_name, const std::string &value, const std::string &desc) : name(name), short_name(short_name), value(value), desc(desc) {}
// };



// 核心转换逻辑封装
int run_convert(std::string input, std::string output, uint32_t chunk_size, char sep, biomxt::DataType dtype, biomxt::CompressAlgo algo, bool first_column_as_rownames)
{
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
    if (!error.empty())
    {
        std::cerr << "Error: " << error << std::endl;
        return 1;
    }
    for (const std::string &warn : warnings)
    {
        std::cerr << "Warning: " << warn << std::endl;
    }

    std::cout << "Row count: " << header.nrow << std::endl;
    std::cout << "Col count: " << header.ncol << std::endl;

    std::cout << "Conversion completed successfully." << std::endl;
    return 0;
}

int main(int argc, char *argv[])
{
    
    cliapp::Command convert = cliapp::Command("convert", "Convert CSV/TSV to BioMXt format")
        .add_argument(cliapp::Argument("input", "Input file path"))
        .add_option(cliapp::Option::option("--output", "-o", "Output file path", ""))
        .add_option(cliapp::Option::option("--chunk-size", "-c", "Chunk size, default: 50k", "50000"))
        .add_option(cliapp::Option::option("--algorithm", "-a", "Compression algorithm: zstd(default), gzip, lz4", "zstd"))
        .add_option(cliapp::Option::option("--separator", "-s", "Separator: ',' or '\\t'. Detect by file extension if not specified, and comma as default if detect failed.", ","))
        .add_option(cliapp::Option::option("--data-type", "-t", "Data type: int16, int32, int64, float32(default), float64", "float32"))
        .add_option(cliapp::Option::option_without_value("--first-column-as-rownames", "-r", "First column as rownames"))
        .add_option(cliapp::Option::option_without_value("--overwrite", "-w", "Overwrite output file if exists"));
    cliapp::App app = cliapp::App("biomxt", "0.1.0", "Lite maxtrix format for bioinformatics")
        .add_option(cliapp::Option::option_without_value("--help", "-h", "Print help message"))
        .add_option(cliapp::Option::option_without_value("--version", "-v", "Print version"))
        .add_command(&convert);

    if(!app.parse(argc, argv)) return 1;

    if (app.got_command(convert)) { 
        // Check if input file exists
        cliapp::Result input = convert.find_provided_argument("input");
        if (!input.success) {
            std::cerr << "Error: Input file path is required." << std::endl;
            return 1;
        }
        if (!std::filesystem::exists(input.value)) {
            std::cerr << "Error: Input file [" << input.value << "] does not exist." << std::endl;
            return 1;
        }

        // Check output file
        cliapp::Result output = convert.find_provided_option("--output");
        if (!output.success) {
            // Change output file extension to .bmxt if not specified
            output.value = fs::path(input.value).replace_extension(".bmxt").string();
        }
        if (std::filesystem::exists(output.value)) {
            cliapp::Result overwrite = convert.find_provided_option("--overwrite");
            if (!overwrite.success) {
                std::cerr << "Error: Output file already exists." << std::endl;
                return 1;
            }
        }
        if (!std::filesystem::exists(fs::path(output.value).parent_path())) {
            if (!std::filesystem::create_directories(fs::path(output.value).parent_path())) {
                std::cerr << "Error: Failed to create output directory." << std::endl;
                return 1;
            }
        }

        // Confirm separator
        char sep = ',';
        cliapp::Result sep_opt = convert.find_provided_option("--separator");
        if (sep_opt.success) {
            if (sep_opt.value == ",") sep = ',';
            else if (sep_opt.value == "\\t") sep = '\t';
            else {
                std::cerr << "Warning: Invalid separator, use comma as default." << std::endl;
                sep = ',';
            }
        } else {
            std::string ext = fs::path(input.value).extension().string();
            if (ext == ".tsv" || ext == ".TSV") {
                sep = '\t';
            }
            else {
                sep = ',';
            }
        }

        // Confirm data type
        biomxt::DataType dtype = biomxt::string_to_dtype(convert.find_provided_option("--data-type").value);

        // Confirm compression algorithm
        biomxt::CompressAlgo algo = biomxt::string_to_algo(convert.find_provided_option("--algorithm").value);

        // Confirm first column as rownames
        bool first_column_as_rownames = convert.find_provided_option("--first-column-as-rownames").success;
        
        // Confirm chunk size
        cliapp::Result chunk_size_str = convert.find_provided_option("--chunk-size");
        if (!chunk_size_str.success) {
            chunk_size_str.value = "50000";
        }
        uint32_t chunk_size = std::stoul(chunk_size_str.value);
        if (chunk_size == 0) {
            std::cerr << "Error: Invalid chunk size." << std::endl;
            return 1;
        }

        // Run conversion
        return run_convert(input.value, output.value, chunk_size, sep, dtype, algo, first_column_as_rownames);

    }

    for (cliapp::Option &opt : app.get_provided_options())
    {
        if (opt.name == "--help" || opt.short_name == "-h")
        {
            app.print_help();
            return 0;
        }
        else if (opt.name == "--version" || opt.short_name == "-v")
        {
            std::cout << app.version << std::endl;
            return 0;
        }
    }

    return 0;
}