#include "biomxt/biomxt_file.hpp"


namespace biomxt {
   
    BiomxtFile::BiomxtFile(const std::string& path, BlockCache* block_cache) {
        // Open file in binary mode for reading
        _ifile.open(path, std::ios::binary);
        if (!_ifile.is_open()) {
            throw std::runtime_error("biomxt::BiomxtFile: Cannot open mmxt file: " + path);
        }

        // If cache_entries is nullptr, use the internal cache_entries
        if (block_cache) {
            _block_cache = block_cache;
        } else {
            _owned_block_cache = std::make_unique<BlockCache>();
            _block_cache = _owned_block_cache.get();
        }

        // Get file size for checking
        _ifile.seekg(0, std::ios::end);
        std::streamsize file_size = _ifile.tellg();
        _ifile.seekg(0, std::ios::beg);

        // Read file header
        if (file_size < sizeof(biomxt::FileHeader)) {
            throw std::runtime_error("biomxt::BiomxtFile: Corrupted file: bad header size");
        }
        _ifile.read(reinterpret_cast<char*>(&_header), sizeof(biomxt::FileHeader));

        // Check magic
        if (std::string(_header.magic, 4) != "BMXt") {
            throw std::runtime_error("biomxt::BiomxtFile: Corrupted file: bad magic: " + std::string(_header.magic, 4));
        }

        // Read block table
        if (_header.block_table_offset >= file_size) {
            throw std::runtime_error("biomxt::BiomxtFile: Corrupted file: block table offset [" + std::to_string(_header.block_table_offset) + "] exceeds file size [" + std::to_string(file_size) + "]");
        }
        _ifile.seekg(_header.block_table_offset, std::ios::beg);
        _block_table.resize(_header.block_count);
        _ifile.read(reinterpret_cast<char*>(_block_table.data()), _header.block_count * sizeof(biomxt::IndexEntry));

        // Calculate max compressed block size
        for (const auto& block_index : _block_table) {
            _max_compressed_block_size = std::max(_max_compressed_block_size, block_index.size);
            _max_uncompressed_block_size = std::max(_max_uncompressed_block_size, block_index.raw_size);
        }
        if (block_cache == nullptr) _block_cache->set_memory_limit(std::max(_header.ncol / _header.block_width, _header.nrow / _header.block_height) * (_max_uncompressed_block_size + sizeof(biomxt::CacheEntry)));

        // Read names table
        if (_header.name_table_offset >= file_size) {
            throw std::runtime_error("Corrupted file: names table offset [" + std::to_string(_header.name_table_offset) + "] exceeds file size [" + std::to_string(file_size) + "]");
        }
        _ifile.seekg(_header.name_table_offset, std::ios::beg);
        std::vector<biomxt::IndexEntry> name_table(_header.nrow + _header.ncol);
        name_table.resize(_header.nrow + _header.ncol);
        _ifile.read(reinterpret_cast<char*>(name_table.data()), (_header.nrow + _header.ncol) * sizeof(biomxt::IndexEntry));

        // Read row names and build map
        _row_names.resize(_header.nrow);
        _row_map.reserve(_header.nrow);
        for (uint32_t i = 0; i < _header.nrow; ++i) {
            _ifile.seekg(name_table[i].offset, std::ios::beg);
            _row_names[i].resize(name_table[i].size);
            _ifile.read(&_row_names[i][0], name_table[i].size);
            _row_map[_row_names[i]] = i;
        }

        // Read column names and build map
        _column_names.resize(_header.ncol);
        _column_map.reserve(_header.ncol);
        for (uint32_t i = 0; i < _header.ncol; ++i) {
            uint32_t idx = _header.nrow + i;
            _ifile.seekg(name_table[idx].offset, std::ios::beg);
            _column_names[i].resize(name_table[idx].size);
            _ifile.read(&_column_names[i][0], name_table[idx].size);
            _column_map[_column_names[i]] = i;
        }
    }

    BiomxtFile::BiomxtFile(const std::string& path) : BiomxtFile(path, nullptr) {}

    BiomxtFile::~BiomxtFile() { _release_resources(); }

    BiomxtFile::BiomxtFile(BiomxtFile&& other) noexcept { *this = std::move(other); }

    BiomxtFile& BiomxtFile::operator=(BiomxtFile&& other) noexcept {
        if (this != &other) {
            // Release old resources
            _release_resources();
            
            // Move resources from other to this
            _ifile = std::move(other._ifile);
            _header = other._header;
            _block_table = std::move(other._block_table);
            _row_names = std::move(other._row_names);
            _column_names = std::move(other._column_names);
            _row_map = std::move(other._row_map);
            _column_map = std::move(other._column_map);
            _max_compressed_block_size = other._max_compressed_block_size;
            _max_uncompressed_block_size = other._max_uncompressed_block_size;

            // Exchange block cache
            _owned_block_cache = std::move(other._owned_block_cache);
            _block_cache = other._block_cache;
            
            // Set other to safty state
            other._header = {}; 
            other._block_cache = nullptr;
        }
        return *this;
    }

    void BiomxtFile::read_block(uint32_t index, std::vector<char>& buffer) {
        // Check file is closed
        if (!_ifile.is_open()) {
            throw std::runtime_error("biomxt::BiomxtFile::read_block: file is closed");
        }

        // Check index range
        if (index >= _header.block_count) {
            throw std::out_of_range("biomxt::BiomxtFile::read_block: block index [" + std::to_string(index) + "] exceeds block count [" + std::to_string(_header.block_count) + "]");
        }

        // Check buffer size
        const auto& block_index = _block_table[index];
        if (buffer.size() != block_index.raw_size) buffer.resize(block_index.raw_size);
        std::vector<char> compressed_buffer(block_index.size);

        // Check cache
        biomxt::BlockKey key = {index, _header.uuid};
        if (_block_cache->get_block_data(key, buffer, 0, block_index.raw_size)) return;

        // Read from file
        _ifile.seekg(block_index.offset, std::ios::beg);
        if (!_ifile.read(compressed_buffer.data(), block_index.size)) {
            throw std::runtime_error("biomxt::BiomxtFile::read_block: read block [" + std::to_string(index) + "] data from file failed");
        }
        
        // Decompress
        size_t decompressed_size = 0;
        switch (_header.algo) {
            case biomxt::CompressAlgorithm::ZSTD:
                decompressed_size = ZSTD_decompress(
                    buffer.data(),                  // target addr
                    block_index.raw_size,           // target size
                    compressed_buffer.data(),       // source addr
                    block_index.size                // source size
                );
                if (ZSTD_isError(decompressed_size)) {
                    throw std::runtime_error("biomxt::BiomxtFile::read_block: ZSTD_decompress error [" + std::string(ZSTD_getErrorName(decompressed_size)) + "]");
                }
                break;
            default:
                throw std::invalid_argument("biomxt::BiomxtFile::read_block: unsupported compression algorithm [" + std::to_string(_header.algo) + "]");
        }

        // Cache block data
        std::vector<char> cache_data(block_index.raw_size);
        std::memcpy(cache_data.data(), buffer.data(), block_index.raw_size);
        _block_cache->insert({index, _header.uuid}, std::move(cache_data));
        
    }

    void BiomxtFile::read_row_data(uint32_t row_index, std::vector<char>& buffer) {
        // Check file is closed
        if (!_ifile.is_open()) {
            throw std::runtime_error("biomxt::BiomxtFile::read_row_data: file is closed");
        }
        
        // Check row index range
        if (row_index >= _header.nrow) {
            throw std::out_of_range("biomxt::BiomxtFile::read_row_data: row index [" + std::to_string(row_index) + "] exceeds row count [" + std::to_string(_header.nrow) + "]");
        }
        
        // Prepare result container
        uint32_t cell_size = biomxt::size_of_dtype(_header.dtype);
        buffer.resize(_header.ncol * cell_size);

        // Calculate block pos
        uint32_t block_pos_y = row_index / _header.block_height; // Block's row index
        uint32_t row_in_block = row_index % _header.block_height;  // Target row index inner block

        // How many block in horizontal direction
        uint32_t block_max_x = (_header.ncol + _header.block_width - 1) / _header.block_width;

        std::vector<char> block_buffer(_header.block_width * _header.block_height * cell_size);

        // Traverse all blocks in horizontal direction
        for (uint32_t block_pos_x = 0; block_pos_x < block_max_x; ++block_pos_x) {
            uint32_t block_idx = (uint32_t)block_pos_y * block_max_x + block_pos_x;
            
            // Read block
            this->read_block(block_idx, block_buffer);

            // Calculate actual block size
            uint32_t actual_block_width = std::min(_header.block_width, _header.ncol - block_pos_x * _header.block_width);

            // Fetch target inner block row
            const char* row_start = block_buffer.data() + (row_in_block * actual_block_width * cell_size);
            std::copy(row_start, row_start + actual_block_width * cell_size, buffer.begin() + (block_pos_x * _header.block_width * cell_size));
        }

    }

    void BiomxtFile::read_row_data(std::string row_name, std::vector<char>& buffer) {
        auto it = _row_map.find(row_name);
        if (it == _row_map.end()) {
            throw std::invalid_argument("biomxt::BiomxtFile::read_row_data: row name [" + row_name + "] not found");
        }
        this->read_row_data(it->second, buffer);
    }

    void BiomxtFile::read_column_data(uint32_t column_index, std::vector<char>& buffer) {
        // Check file is closed
        if (!_ifile.is_open()) {
            throw std::runtime_error("biomxt::BiomxtFile::read_column_data: file is closed");
        }
        
        // Check row index range
        if (column_index >= _header.ncol) {
            throw std::out_of_range("biomxt::BiomxtFile::read_column_data: column index [" + std::to_string(column_index) + "] exceeds column count [" + std::to_string(_header.ncol) + "]");
        }
        
        // Prepare result container
        uint32_t cell_size = biomxt::size_of_dtype(_header.dtype);
        buffer.resize(_header.nrow * cell_size);
        // Prepare block buffer
        std::vector<char> block_buffer(_header.block_width * _header.block_height * cell_size);

        // Calculate block pos
        uint32_t block_pos_x = column_index / _header.block_width; // Block's column index
        uint32_t col_in_block = column_index % _header.block_width;  // Target column index inner block

        // How many block in vertical direction
        uint32_t block_max_x = (_header.ncol + _header.block_width - 1) / _header.block_width;
        uint32_t block_max_y = (_header.nrow + _header.block_height - 1) / _header.block_height;

        // Traverse all blocks in vertical direction
        for (uint32_t block_pos_y = 0; block_pos_y < block_max_y; ++block_pos_y) {
            uint32_t block_idx = (uint32_t)block_pos_y * block_max_x + block_pos_x;
            
            // Read block
            this->read_block(block_idx, block_buffer);

            // Calculate actual block size
            uint32_t actual_block_width = std::min(_header.block_width, _header.ncol - block_pos_x * _header.block_width);
            uint32_t actual_block_height = block_buffer.size() / cell_size / actual_block_width;

            // Fetch target inner block col
            char* block_ptr = block_buffer.data() + col_in_block * cell_size; // Target column's first row in block
            uint64_t cell_offset = (uint64_t)block_pos_y * _header.block_height * cell_size; // Calculate cell offset in result cells

            for (uint32_t i = 0; i < actual_block_height; ++i) {
                std::memcpy(
                    buffer.data() + cell_offset + i * cell_size, 
                    block_ptr, 
                    cell_size);
                block_ptr += actual_block_width * cell_size; // Jump to next row inner block
            }
        }

    }

    void BiomxtFile::read_column_data(std::string column_name, std::vector<char>& buffer) {
        auto it = _column_map.find(column_name);
        if (it == _column_map.end()) {
            throw std::invalid_argument("biomxt::BiomxtFile::read_column_data: column name [" + column_name + "] not found");
        }
        return read_column_data(it->second, buffer);
    }

    const std::vector<std::string>& BiomxtFile::get_row_names() const {
        if (!_ifile.is_open()) {
            throw std::runtime_error("biomxt::BiomxtFile::get_row_names: File has been closed.");
        }
        return _row_names;
    }
    
    std::vector<std::string> BiomxtFile::get_row_names(const std::vector<uint32_t>& row_indices) const {
        if (!_ifile.is_open()) {
            throw std::runtime_error("biomxt::BiomxtFile::get_row_names: File has been closed.");
        }
        // Reserve
        std::vector<std::string> results;
        results.reserve(row_indices.size()); 
        
        // Fill
        for (uint32_t idx : row_indices) {
            if (idx < _header.nrow) {
                results.push_back(_row_names[idx]);
            } else {
                throw std::runtime_error("biomxt::BiomxtFile::get_row_names: Row index out of range: " + std::to_string(idx));
            }
        }
        return results;
    }

    const std::vector<std::string>& BiomxtFile::get_column_names() const { 
        if (!_ifile.is_open()) {
            throw std::runtime_error("biomxt::BiomxtFile::get_column_names: File has been closed.");
        }
        return _column_names;
    }
    
    std::vector<std::string> BiomxtFile::get_column_names(const std::vector<uint32_t>& column_indices) const {
        if (!_ifile.is_open()) {
            throw std::runtime_error("biomxt::BiomxtFile::get_column_names: File has been closed.");
        }
        // Reserve
        std::vector<std::string> results;
        results.reserve(column_indices.size()); 
        
        // Fill
        for (uint32_t idx : column_indices) {
            if (idx < _header.ncol) {
                results.push_back(_column_names[idx]);
            } else {
                throw std::runtime_error("biomxt::BiomxtFile::get_column_names: Column index out of range: " + std::to_string(idx));
            }
        }
        return results;
    }
    
    std::vector<uint32_t> BiomxtFile::get_row_indices(const std::vector<std::string>& row_names) const {
        if (!_ifile.is_open()) {
            throw std::runtime_error("biomxt::BiomxtFile::get_row_indices: File has been closed.");
        }

        std::vector<uint32_t> results;
        // Reserve
        results.reserve(row_names.size());

        for (const auto& name : row_names) {
            // Locate via map
            auto it = _row_map.find(name);
            
            // Not found
            if (it == _row_map.end()) {
                throw std::runtime_error("biomxt::BiomxtFile::get_row_indices: Row name not found: " + name);
            }

            // Found
            results.push_back(it->second);
        }
        
        return results;
    }
    
    std::vector<uint32_t> BiomxtFile::get_column_indices(const std::vector<std::string>& column_names) const {
        if (!_ifile.is_open()) {
            throw std::runtime_error("biomxt::BiomxtFile::get_column_indices: File has been closed.");
        }
        // Reserve
        std::vector<uint32_t> results;
        results.reserve(column_names.size()); 
        
        // Fill
        for (const auto& name : column_names) {
            // Locate via map
            auto it = _column_map.find(name);
            
            // Not found
            if (it == _column_map.end()) {
                throw std::runtime_error("biomxt::BiomxtFile::get_column_indices: Column name not found: " + name);
            }

            // Found
            results.push_back(it->second);
        }
        
        return results;
    }

    biomxt::FileHeader& BiomxtFile::get_header() { 
        if (!_ifile.is_open()) {
            throw std::runtime_error("biomxt::BiomxtFile::get_header: File has been closed.");
        }
        return _header;
    }

    void BiomxtFile::close() { _release_resources(); }

    uint32_t BiomxtFile::get_max_compressed_block_size() const { return _max_compressed_block_size; }

    uint32_t BiomxtFile::get_max_uncompressed_block_size() const { return _max_uncompressed_block_size; }

    uint32_t BiomxtFile::get_block_cache_memory_limit() const { return _block_cache->get_memory_limit(); }
}