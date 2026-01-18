#include "biomxt/biomxt.hpp"


namespace biomxt {
    
    biomxt::IndexEntry compress_bytes_and_write(
        const void* src_data, 
        size_t uncompressed_byte_size, 
        std::ofstream& out, 
        std::vector<char>& compressed_buf,
        biomxt::CompressAlgo algo) 
    {
        if (!src_data || uncompressed_byte_size == 0) {
            throw std::invalid_argument("biomxt::compress_bytes_and_write: Source data is null or empty.");
        }

        biomxt::IndexEntry entry;
        entry.offset = static_cast<uint64_t>(out.tellp());
        entry.uncompressed_size = static_cast<uint32_t>(uncompressed_byte_size);

        size_t compressed_size = 0;

        if (algo == biomxt::CompressAlgo::ZSTD) {
            size_t max_dst_size = ZSTD_compressBound(uncompressed_byte_size);
            if (compressed_buf.size() < max_dst_size) {
                compressed_buf.resize(max_dst_size);
            }

            compressed_size = ZSTD_compress(
                compressed_buf.data(), max_dst_size, 
                src_data, uncompressed_byte_size, 3
            );

            if (ZSTD_isError(compressed_size)) {
                throw std::runtime_error("biomxt::compress_bytes_and_write: ZSTD error: " + std::string(ZSTD_getErrorName(compressed_size)));
            }
        } else {
            throw std::invalid_argument("biomxt::compress_bytes_and_write: Unsupported compress algo: " + biomxt::algo_to_string(algo));
        }

        // Write compressed data to file
        out.write(compressed_buf.data(), compressed_size);

        // Record compressed size in index entry
        entry.size = static_cast<uint32_t>(compressed_size);
        
        return entry;
    }


    template <typename T> void flush_buffer(
        const std::vector<std::vector<T>>& rows_buffer, 
        uint32_t block_width, 
        uint32_t actual_block_height,
        std::vector<biomxt::IndexEntry>& block_table,
        std::ofstream& out,
        std::vector<T>& block,
        std::vector<char>& compress_buffer,
        biomxt::CompressAlgo algo) 
    {
        // Check buffer validity
        if (rows_buffer.empty()) {
            throw std::invalid_argument("biomxt::flush_buffer: Buffer is empty.");
        }
        
        uint64_t row_buffer_size = rows_buffer[0].size();
        uint64_t steps = 0;
        uint32_t actual_block_width = 0;
        for(uint64_t pos = 0; pos < row_buffer_size; pos++) {
            // Steps == 0 means it goes into a new block
            if (steps == 0) {
                actual_block_width = std::min(block_width, static_cast<uint32_t>(row_buffer_size-pos));
                if (block.size() != actual_block_width*actual_block_height) {
                    block.resize(actual_block_width*actual_block_height);
                }
            }
            // Fill the block
            for (uint64_t i=0; i<actual_block_height; i++){
                block[steps+actual_block_width*i] = rows_buffer[i][pos];
            }
            steps++;

            // Steps == actual_block_width-1 means the block is full filled.
            if (steps == actual_block_width) {
                block_table.push_back(compress_bytes_and_write(block.data(), actual_block_width*actual_block_height*sizeof(T), out, compress_buffer, algo));
                steps = 0;
            }
        }
    }

    template <typename T>biomxt::FileHeader csv_to_bmxt(
        const std::string& input_file, 
        const std::string& output_file, 
        uint32_t block_width, 
        uint32_t block_height, 
        char separator, 
        biomxt::CompressAlgo algo,
        std::vector<std::string>& warnings) {

            static_assert(biomxt::dtype_from_type<T>::valid, "biomxt::csv_to_bmxt: Invalid data type.");
            warnings.clear();

            // Create file header and fill some basic information
            biomxt::FileHeader header; 
            header.version = biomxt::VERSION;
            header.dtype = biomxt::dtype_from_type<T>::value;
            header.algo = algo;
            header.block_width = block_width;
            header.block_height = block_height;

            // Create output file
            std::ofstream out_file(output_file, std::ios::binary);
            if (!out_file.is_open()) throw std::runtime_error("biomxt::csv_to_bmxt: Failed to open output file: " + output_file);

            // Reserve space for header
            out_file.seekp(sizeof(biomxt::FileHeader));

            // Open input file
            std::ifstream in_file(input_file);
            if (!in_file.is_open()) throw std::runtime_error("biomxt::csv_to_bmxt: Failed to open input file: " + input_file);

            std::vector<std::string> colnames;
            std::vector<std::string> rownames;

            std::vector<std::vector<T>> rows_buffer(block_height);
            size_t actual_block_height = 0;
            std::vector<T> block;
            std::vector<biomxt::IndexEntry> block_table;
            std::vector<char> compress_buffer;
            std::vector<T> block_buffer;

            std::string line;
            uint64_t cur_file_line = 0;
            uint64_t reserve_size = 0;
            
            // Streamly read and write
            while (std::getline(in_file, line)) {
                cur_file_line++;
                // Skip empty line
                if (line.empty() || line[0] == '#') {
                    continue;
                }
                
                // Parse line and skip empty lines
                std::vector<std::string> cells = parse_line(line, reserve_size, separator);
                if (cells.empty()) {
                    continue;
                }


                // Found headers
                if (colnames.empty()) {
                    // Skip first header
                    for(size_t i = 1; i < cells.size(); ++i) colnames.push_back(cells[i]);
                    header.ncol = colnames.size();
                    // Initialize rows buffer
                    for (std::vector<T>& row : rows_buffer) {
                        row.resize(colnames.size());
                    }
                    continue;
                }

                // Check cell count
                if (cells.size() != colnames.size()+1) {
                    throw std::runtime_error("biomxt::csv_to_bmxt: Line " + std::to_string(cur_file_line) + " has " + std::to_string(cells.size()-1) + " cells (rowname excluded), expected " + std::to_string(colnames.size()-1) + " cells.");
                    continue;
                }

                // First cell is row name
                rownames.push_back(cells[0]);

                // Convert cells and add to rows buffer
                for (size_t i = 1; i < cells.size(); i++) {
                    rows_buffer[actual_block_height][i-1] = biomxt::string_to<T>(cells[i]);
                }
                actual_block_height++;

                // Flush rows buffer when it is full
                if (actual_block_height == block_height) {
                    flush_buffer(rows_buffer, block_width, actual_block_height, block_table, out_file, block_buffer, compress_buffer, algo);
                    actual_block_height = 0;
                }
            }

            // Input file read done
            in_file.close();

            // Catch the last block
            if (actual_block_height > 0) {
                flush_buffer(rows_buffer, block_width, actual_block_height, block_table, out_file, block_buffer, compress_buffer, algo);
            }

            header.nrow = rownames.size();
            header.ncol = colnames.size();
            
            // Write names to output file
            std::vector<biomxt::IndexEntry> names_table;
            for (const std::string& name : rownames) {
                names_table.push_back({static_cast<uint64_t>(out_file.tellp()), (uint32_t)name.size(), (uint32_t)name.size()});
                out_file.write(name.data(), name.size());
            }
            for (const std::string& name : colnames) {
                names_table.push_back({static_cast<uint64_t>(out_file.tellp()), (uint32_t)name.size(), (uint32_t)name.size()});
                out_file.write(name.data(), name.size());
            }

            // Write block count and table 
            header.block_count = block_table.size();
            header.blocks_table_offset = out_file.tellp();
            out_file.write(reinterpret_cast<const char*>(block_table.data()), block_table.size() * sizeof(biomxt::IndexEntry));

            // Write names table
            header.names_table_offset = out_file.tellp();
            out_file.write(reinterpret_cast<const char*>(names_table.data()), names_table.size() * sizeof(biomxt::IndexEntry));

            // Write header to output file
            out_file.seekp(0);
            out_file.write(reinterpret_cast<const char*>(&header), sizeof(biomxt::FileHeader));

            // Write done
            out_file.close();
            return header;

    }

    // --- 重要：显式实例化 ---
    template biomxt::FileHeader csv_to_bmxt<int16_t>(const std::string&, const std::string&, uint32_t, uint32_t, char, CompressAlgo, std::vector<std::string>&);
    template biomxt::FileHeader csv_to_bmxt<int32_t>(const std::string&, const std::string&, uint32_t, uint32_t, char, CompressAlgo, std::vector<std::string>&);
    template biomxt::FileHeader csv_to_bmxt<int64_t>(const std::string&, const std::string&, uint32_t, uint32_t, char, CompressAlgo, std::vector<std::string>&);
    template biomxt::FileHeader csv_to_bmxt<float>(const std::string&, const std::string&, uint32_t, uint32_t, char, CompressAlgo, std::vector<std::string>&);
    template biomxt::FileHeader csv_to_bmxt<double>(const std::string&, const std::string&, uint32_t, uint32_t, char, CompressAlgo, std::vector<std::string>&);
}