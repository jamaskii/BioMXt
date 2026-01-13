#include "biomxt/biomxt.hpp"


namespace biomxt {
    bool chunk_flush(
        std::vector<float>& chunk, 
        std::ofstream& out, 
        std::vector<biomxt::IndexEntry>& chunk_indices, 
        biomxt::CompressAlgo algo) {

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

    biomxt::FileHeader csv_to_bmxt(
        const std::string& input_file, 
        const std::string& output_file, 
        bool first_col_as_rownames, 
        uint64_t chunk_size, 
        char separation, 
        biomxt::DataType dtype, 
        biomxt::CompressAlgo algo, 
        std::string& error, 
        std::vector<std::string>& warnings) {

            biomxt::FileHeader header; 
            error.clear();
            warnings.clear();
            // Create output file
            std::ofstream out_file(output_file, std::ios::binary);
            if (!out_file.is_open()) {
                error = "Failed to open output file: " + output_file;
                return header;
            }

            // Reserve space for header
            out_file.seekp(sizeof(biomxt::FileHeader));

            // Open input file
            std::ifstream in_file(input_file);
            if (!in_file.is_open()) {
                error = "Failed to open input file: " + input_file;
                return header;
            }

            // Parse input file and write to output file
            uint64_t input_file_line = 1;
            uint64_t col_counts = 0;
            uint64_t row_counts = 0;
            std::vector<std::string_view> rownames;
            std::vector<std::string_view> colnames;
            uint64_t reserve_size = 0;
            std::vector<float> chunk;
            chunk.reserve(chunk_size);
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
                std::vector<std::string_view> cells = biomxt::parse_line(line, reserve_size);

                // Colnames line
                if (col_counts == 0) {
                    // Check format
                    if (cells.size() < 2 && (!cells[0].empty() && !first_col_as_rownames)) {
                        error = "Format error: First cell in colnames line must be empty, representing the rownames column.";
                        return header;
                    }
                    // Fetch colnames
                    cells.erase(cells.begin());
                    colnames = cells;
                    col_counts = cells.size();

                // Common line
                } else {
                    // Check format
                    if (cells.size() != col_counts+1) {
                        warnings.push_back("Format warning: Line " + std::to_string(input_file_line) + " was ignored, since it has " + std::to_string(cells.size()) + " cells (rowname excluded), but expected " + std::to_string(col_counts+1) + ".");
                        input_file_line++;
                        continue;
                    }

                    // Fetch rowname
                    rownames.push_back(std::move(cells[0]));

                    // Push cells to chunk
                    // Initialize cell_chunked to 1, since the first cell is rowname
                    uint64_t cell_chunked = 1;
                    while (cell_chunked < cells.size()) {
                        uint64_t chunk_free = chunk_size - chunk.size();

                        // Flush chunk if full
                        if (chunk_free == 0) {
                            if (!biomxt::chunk_flush(chunk, out_file, chunk_indices, algo)) {
                                error = "Failed to flush chunk at line " + std::to_string(input_file_line);
                                return header;
                            }
                        }

                        // Ensure count of cells to be pushed to chunk will not exceed chunk size
                        uint64_t push_size = std::min(chunk_free, cells.size() - cell_chunked);
                        
                        // Push cells to chunk
                        for (uint64_t i=0; i<push_size; ++i) {
                            chunk.push_back(biomxt::fast_atof(cells[cell_chunked+i]));
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
                if (!biomxt::chunk_flush(chunk, out_file, chunk_indices, algo)) {
                    error = "Failed to flush chunk at line " + std::to_string(input_file_line);
                    return header;
                }
                chunk.clear();
            }

            // Create file header
            
            header.nrow = (uint32_t)row_counts;
            header.ncolumn = (uint32_t)col_counts;
            header.chunk_size = chunk_size;
            header.dtype = dtype;
            header.algo = algo;

            // Write names to output file
            std::vector<biomxt::IndexEntry> names_indices;
            for (const std::string_view& name : rownames) {
                names_indices.push_back({static_cast<uint64_t>(out_file.tellp()), (uint32_t)name.size()});
                out_file.write(name.data(), name.size());
            }
            for (const std::string_view& name : colnames) {
                names_indices.push_back({static_cast<uint64_t>(out_file.tellp()), (uint32_t)name.size()});
                out_file.write(name.data(), name.size());
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
            return header;
    }

}