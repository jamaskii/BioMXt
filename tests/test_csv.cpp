#include <iostream>
#include <cassert>
#include "biomxt/csv_parser.hpp"


void parse_and_show(const std::string& line, char separation = ',') {
    uint64_t reserve_size = 0;
    std::vector<std::string> cells = biomxt::parse_line(line, reserve_size, separation);
    std::cout << "Line: [" << line << "]" << std::endl;
    std::cout << "Parsed Cells Count: " << cells.size() << std::endl;
    for(size_t i=0; i<cells.size(); ++i) {
        std::cout << "Cell [" << i << "]: [" << cells[i] << "]" << std::endl;
    }
    std::cout << std::endl;
}

int main() {
    parse_and_show("");
    parse_and_show(" ");
    parse_and_show("\"\"");
    parse_and_show(",");
    parse_and_show("\"Gene A\",1.23,\"Cell \"\"Alpha\"\"\", ,-0.5e-10");
    parse_and_show("\"Gene A\",1.23,\"Cell \"\"Alpha\", ,\"-0.5e-10");
    parse_and_show("\"Gene A\";1.23;\"Cell \"\"Alpha\"\"\"; ;-0.5e-10", ';');
    parse_and_show("\"Gene A\"\t1.23\t\"Cell \"\"Alpha\"\"\"\t \t-0.5e-10", '\t');
    return 0;
}