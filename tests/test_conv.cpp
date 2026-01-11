#include <iostream>
#include <fstream>
#include <vector>
#include "biomxt/csv_parser.hpp"
#include "biomxt/spec.hpp"
#include "zstd.h"


#define ARG_INPUT_FILE              "test_data/PRJNA978570_RNA_data.csv"
#define ARG_OUTPUT_FILE             "test_data/PRJNA978570_RNA_data.bmxt"
#define ARG_FIRST_COL_AS_ROWNAMES   true
#define ARG_CHUNK_SIZE              50000
#define ARG_SEPARATION_CHAR         ','
#define ARG_DTYPE                   biomxt::DataType::FLOAT32
#define ARG_ALGO                    biomxt::CompressAlgo::ZSTD


bool chunk_flush(std::vector<float>& chunk, std::ofstream& out, std::vector<biomxt::IndexEntry>& chunk_indices) {
    if (chunk.empty()) return true;

    // Mark current position as chunk start
    biomxt::IndexEntry entry;
    entry.offset = (uint64_t)out.tellp();

    // Zstd
    size_t src_size = chunk.size() * sizeof(float);
    size_t max_dst_size = ZSTD_compressBound(src_size);
    std::vector<char> compressed_buf(max_dst_size);

    size_t c_size = ZSTD_compress(compressed_buf.data(), max_dst_size, 
                                  chunk.data(), src_size, 3);
    
    if (ZSTD_isError(c_size)) return false;

    // Write to file
    out.write(compressed_buf.data(), c_size);

    // Record index
    entry.size = (uint32_t)c_size;
    chunk_indices.push_back(entry);
    
    // Clear chunk buffer
    chunk.clear();
    return true;
}

void print_bmxt_header(const biomxt::FileHeader& header) {
    std::cout << "Magic: " << std::string_view(header.magic, 4) << std::endl;
    std::cout << "Version: " << header.version << std::endl;
    std::cout << "DataType: " << (int)header.dtype << std::endl;
    std::cout << "CompressAlgo: " << (int)header.algo << std::endl;
    std::cout << "NRow: " << header.nrow << std::endl;
    std::cout << "NCol: " << header.ncol << std::endl;
    std::cout << "ChunkSize: " << header.chunk_size << std::endl;
    std::cout << "ChunkTableOffset: " << header.chunk_table_offset << std::endl;
    std::cout << "NamesTableOffset: " << header.names_table_offset << std::endl;
}


int main() {
    // Create output file
    std::ofstream out_file(ARG_OUTPUT_FILE, std::ios::binary);
    if (!out_file.is_open()) {
        std::cerr << "Failed to open output file: " << ARG_OUTPUT_FILE << std::endl;
        return 1;
    }

    // Reserve space for header
    out_file.seekp(sizeof(biomxt::FileHeader));

    // Open input file
    std::ifstream in_file(ARG_INPUT_FILE);
    if (!in_file.is_open()) {
        std::cerr << "Failed to open CSV file: " << ARG_INPUT_FILE << std::endl;
        return 1;
    }

    // Parse input file and write to output file
    uint64_t input_file_line = 1;
    uint64_t col_counts = 0;
    uint64_t row_counts = 0;
    std::vector<std::string> rownames;
    std::vector<std::string> colnames;
    uint64_t reserve_size = 0;
    std::vector<float> chunk;
    chunk.reserve(ARG_CHUNK_SIZE);
    std::vector<biomxt::IndexEntry> chunk_indices;
    
    std::string line;
    while (std::getline(in_file, line)) {
        // Skip empty line
        if (line.empty()) {
            input_file_line++; 
            continue;
        }
        // Skip comment line
        if (line[0] == '#') {
            input_file_line++; 
            continue;
        }

        // Parse line
        std::vector<std::string> cells = biomxt::parse_line(line, reserve_size);

        // Colnames line
        if (col_counts == 0) {
            // Check format
            if (cells.size() < 2 && (!cells[0].empty() && !ARG_FIRST_COL_AS_ROWNAMES)) {
                std::cerr << "Format error: First cell in colnames line must be empty, means the rownames column." << std::endl;
                return 1;
            }
            // Fetch colnames
            cells.erase(cells.begin());
            colnames = cells;
            col_counts = cells.size();

        // Common line
        } else {
            // Check format
            if (cells.size() != col_counts+1) {
                std::cerr << "Format warning: [Line " << input_file_line << "] was ignored, since it has " << cells.size() << " cells (rowname excluded), but expected " << col_counts+1 << "." << std::endl;
                input_file_line++;
                continue;
            }

            // Fetch rowname
            rownames.push_back(std::move(cells[0]));
            // cells.erase(cells.begin());

            // Push cells to chunk
            // Initialize cell_chunked to 1, since the first cell is rowname
            uint64_t cell_chunked = 1;
            while (cell_chunked < cells.size()) {
                uint64_t chunk_free = ARG_CHUNK_SIZE - chunk.size();

                // Flush chunk if full
                if (chunk_free == 0) {
                    if (!chunk_flush(chunk, out_file, chunk_indices)) {
                        std::cerr << "Failed to flush chunk at line " << input_file_line << std::endl;
                        return 1;
                    }
                }

                // Ensure count of cells to be pushed to chunk will not exceed chunk size
                uint64_t push_size = std::min(chunk_free, cells.size() - cell_chunked);
                
                // Push cells to chunk
                for (uint64_t i=0; i<push_size; ++i) {
                    chunk.push_back(biomxt::fast_atof(cells[cell_chunked+i].c_str()));
                }
                cell_chunked += push_size;
            }
            
            row_counts++;
        }

        input_file_line++;
    }

    // Input file read done
    in_file.close();

    // Flush chunk if not empty
    if (!chunk.empty()) {
        if (!chunk_flush(chunk, out_file, chunk_indices)) {
            std::cerr << "Failed to flush chunk at line " << input_file_line << std::endl;
            return 1;
        }
        chunk.clear();
    }

    // Create file header
    biomxt::FileHeader header; 
    header.nrow = (uint32_t)row_counts;
    header.ncol = (uint32_t)col_counts;
    header.chunk_size = ARG_CHUNK_SIZE;
    header.dtype = ARG_DTYPE;
    header.algo = ARG_ALGO;

    // Write names to output file
    std::vector<biomxt::IndexEntry> names_indices;
    for (const std::string& name : colnames) {
        names_indices.push_back({static_cast<uint64_t>(out_file.tellp()), (uint32_t)name.size()});
        out_file.write(name.c_str(), name.size());
    }
    for (const std::string& name : rownames) {
        names_indices.push_back({static_cast<uint64_t>(out_file.tellp()), (uint32_t)name.size()});
        out_file.write(name.c_str(), name.size());
    }

    // Write chunk index table to output file
    header.chunk_table_offset = out_file.tellp();
    out_file.write((const char*)chunk_indices.data(), chunk_indices.size() * sizeof(biomxt::IndexEntry));

    // Write names index table to output file
    header.names_table_offset = out_file.tellp();
    out_file.write((const char*)names_indices.data(), names_indices.size() * sizeof(biomxt::IndexEntry));

    // Write header to output file
    out_file.seekp(0);
    out_file.write((const char*)&header, sizeof(biomxt::FileHeader));

    // Write done
    out_file.close();
    print_bmxt_header(header);

    return 0;
}