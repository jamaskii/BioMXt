#include "biomxt/utils/csv_parser.hpp"


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
}