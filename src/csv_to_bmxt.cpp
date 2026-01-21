#include "biomxt/csv_to_bmxt.hpp"


namespace biomxt {

    uint32_t csv_parse_line(
        const std::string& line, 
        std::vector<std::string>& cells,
        const char separator)
        {
            // Locate the actual end posotion, exclude \r or \n.
            size_t end_pos = line.size();
            while (end_pos > 0 && (line[end_pos - 1] == '\r' || line[end_pos - 1] == '\n')) {
                end_pos--;
            }

            // If it is empty line
            if (end_pos == 0) return 0;

            // Status: whether we are in a quote.
            bool in_quote = false;

            // Parse the line char by char
            uint32_t cell_count = 0;
            const size_t max_cells = cells.size();
            if (max_cells == 0) {
                throw std::invalid_argument("biomxt::csv_parse_line: Size of cells vector cannot be zero");
            }
            cells[0].clear();
            for (size_t i = 0; i < end_pos; ++i) {
                const char c = line[i];
                // Detect double quote escape.
                if (c == '"') {
                    if (!in_quote){
                        // Enter quote mode.
                        in_quote = true;
                    } else if ( i + 1 < end_pos && line[i+1] == '"'){
                        // Current char is double quote, append it and skip next quote.
                        cells[cell_count] += '"';
                        ++i;
                    } else {
                        in_quote = false;
                    }
                } else if (c == separator && !in_quote) {
                    // Encounter separator, and not in quote mode, move to next cell.
                    cell_count++;

                    // Check container size.
                    if (cell_count >= max_cells) {
                        throw std::out_of_range("biomxt::csv_parse_line: This line contains too much cells, exceeds cells vector size: " + std::to_string(max_cells));
                    }
                    cells[cell_count].clear();
                } else {
                    // Current char neither separator nor quote, append it directly to cell.
                    cells[cell_count] += line[i];
                }
            }

            if (in_quote) {
                throw std::invalid_argument("biomxt::csv_parse_line: This line contains unclosed quote.");
            }

            // There will not be a separator at the end of last cell, so we need to add the count manually for it.
            return cell_count + 1;
        }

    uint32_t csv_parse_line(
        const std::string& line, 
        const char separator) 
        {
            // Locate the actual end posotion, exclude \r or \n.
            size_t end_pos = line.size();
            while (end_pos > 0 && (line[end_pos - 1] == '\r' || line[end_pos - 1] == '\n')) {
                end_pos--;
            }

            // If it is empty line
            if (end_pos == 0) return 0;

            // Status: whether we are in a quote.
            bool in_quote = false;

            // Parse the line char by char
            uint32_t cell_count = 0;
            for (size_t i = 0; i < end_pos; ++i) {
                const char c = line[i];
                // Detect double quote escape.
                if (c == '"') {
                    if (!in_quote){
                        // Enter quote mode.
                        in_quote = true;
                    } else if ( i + 1 < end_pos && line[i+1] == '"'){
                        // Current char is double quote, append it and skip next quote.
                        // cell += '"';
                        ++i;
                    } else {
                        in_quote = false;
                    }
                } else if (c == separator && !in_quote) {
                    // Encounter separator, and not in quote mode, means a cell end.
                    // cells.push_back(std::move(cell));
                    // cell.clear();
                    cell_count++;
                } else {
                    // Current char neither separator nor quote, append it to cell.
                    // cell += line[i];
                }
            }

            if (in_quote) {
                throw std::invalid_argument("biomxt::csv_parse_line: This line contains unclosed quote.");
            }

            cell_count++;
            return cell_count;
        }

    template <typename T> void flush_rows_buffer(
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
        
        // Split rows buffer into blocks
        uint32_t row_buffer_size = rows_buffer[0].size();
        uint32_t steps = 0;
        uint32_t actual_block_width = 0;
        for(uint32_t pos = 0; pos < row_buffer_size; pos++) {
            // When goes into a new block, update actual block width.
            if (steps == 0) {
                actual_block_width = std::min(block_width, static_cast<uint32_t>(row_buffer_size-pos));
                if (block.size() != actual_block_width*actual_block_height) {
                    block.resize(actual_block_width*actual_block_height);
                }
            }

            // Fill the block with current column.
            for (uint32_t i=0; i<actual_block_height; i++){
                block[actual_block_width*i+steps] = rows_buffer[i][pos];
            }
            steps++;

            // When block is full filled, compress it and write to file.
            if (steps == actual_block_width) {
                biomxt::IndexEntry entry;
                entry.offset = out.tellp();
                entry.raw_size = block.size()*sizeof(T);

                // Compress
                size_t dst_size = 0;
                switch (algo) {
                    case biomxt::CompressAlgo::ZSTD:
                        dst_size = ZSTD_compressBound(entry.raw_size);
                        if (dst_size > compress_buffer.size()) {
                            compress_buffer.resize(dst_size);
                        }
                        dst_size = ZSTD_compress(compress_buffer.data(), dst_size, block.data(), entry.raw_size, 3);
                        if (ZSTD_isError(dst_size)) {
                            throw std::runtime_error("biomxt::flush_buffer: ZSTD_compress failed.");
                        }
                        entry.size = dst_size;
                        break;
                    default:
                        throw std::invalid_argument("biomxt::flush_buffer: Unsupported compression algorithm.");
                }

                // Write to file
                out.write(compress_buffer.data(), entry.size);
                block_table.push_back(entry);

                // Next show go to new block.
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

            // Check block width and height
            if (block_height == 0 || block_width == 0) {
                throw std::invalid_argument("biomxt::csv_to_bmxt: Block width or height must be greater than 0.");
            }

            // Create file header and fill some basic information
            biomxt::FileHeader header; 
            header.dtype = biomxt::dtype_from_type<T>::value;
            header.algo = algo;
            header.block_width = block_width;
            header.block_height = block_height;
            header.uuid = biomxt::UUID::generate();

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
            std::vector<std::string> parse_buffer;
            size_t actual_block_height = 0;
            std::vector<T> block;
            std::vector<biomxt::IndexEntry> block_table;
            std::vector<char> compress_buffer;
            std::vector<T> block_buffer;

            std::string line;
            uint32_t cur_file_line = 0;
            uint32_t reserve_size = 0;
            
            // Streamly read and write
            while (std::getline(in_file, line)) {
                cur_file_line++;

                // Skip empty line
                if (line.empty() || line[0] == '#') {
                    continue;
                }

                // First non-empty line as the header
                if (colnames.empty()) {
                    // Get ncol
                    uint32_t ncol = biomxt::csv_parse_line(line, separator);
                    // Initialize rows buffer by ncol-1 (Ignore first column which is row name)
                    for (std::vector<T>& row : rows_buffer) {
                        row.resize(ncol-1);
                    }
                    parse_buffer.resize(ncol);
                    // Fetch colnames
                    colnames.resize(ncol);
                    biomxt::csv_parse_line(line, colnames, separator);
                    colnames.erase(colnames.begin());
                    continue;
                }

                // Non-first non-empty line as the data
                size_t ncell = biomxt::csv_parse_line(line, parse_buffer, separator);
                if (ncell != colnames.size()+1) {
                    throw std::runtime_error("biomxt::csv_to_bmxt: Line " + std::to_string(cur_file_line) + " has " + std::to_string(ncell-1) + " cells (rowname excluded), expected " + std::to_string(colnames.size()) + " cells.");
                }

                // First cell is row name
                rownames.push_back(parse_buffer[0]);

                // Convert cells and add to rows buffer
                for (size_t i = 1; i < parse_buffer.size(); i++) {
                    rows_buffer[actual_block_height][i-1] = biomxt::string_to<T>(parse_buffer[i]);
                }
                actual_block_height++;

                // Flush rows buffer when it is full
                if (actual_block_height == block_height) {
                    flush_rows_buffer(rows_buffer, block_width, actual_block_height, block_table, out_file, block_buffer, compress_buffer, algo);
                    actual_block_height = 0;
                }
            }

            // Input file read done
            in_file.close();

            // Catch the last block
            if (actual_block_height > 0) {
                flush_rows_buffer(rows_buffer, block_width, actual_block_height, block_table, out_file, block_buffer, compress_buffer, algo);
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
            header.block_table_offset = static_cast<uint64_t>(out_file.tellp());
            out_file.write(reinterpret_cast<const char*>(block_table.data()), block_table.size() * sizeof(biomxt::IndexEntry));

            // Write names table
            header.name_table_offset = static_cast<uint64_t>(out_file.tellp());
            out_file.write(reinterpret_cast<const char*>(names_table.data()), names_table.size() * sizeof(biomxt::IndexEntry));

            // Write header to output file
            out_file.seekp(0);
            out_file.write(reinterpret_cast<const char*>(&header), sizeof(biomxt::FileHeader));

            // Write done
            out_file.close();
            return header;

    }

    template biomxt::FileHeader csv_to_bmxt<int16_t>(const std::string&, const std::string&, uint32_t, uint32_t, char, CompressAlgo, std::vector<std::string>&);
    template biomxt::FileHeader csv_to_bmxt<int32_t>(const std::string&, const std::string&, uint32_t, uint32_t, char, CompressAlgo, std::vector<std::string>&);
    template biomxt::FileHeader csv_to_bmxt<int64_t>(const std::string&, const std::string&, uint32_t, uint32_t, char, CompressAlgo, std::vector<std::string>&);
    template biomxt::FileHeader csv_to_bmxt<float>(const std::string&, const std::string&, uint32_t, uint32_t, char, CompressAlgo, std::vector<std::string>&);
    template biomxt::FileHeader csv_to_bmxt<double>(const std::string&, const std::string&, uint32_t, uint32_t, char, CompressAlgo, std::vector<std::string>&);
}