#pragma once
#include <cstdint>
#include <vector>


namespace biomxt
{
    template <typename T>
    struct Cells {
        public:
            // Constructor
            Cells(const std::vector<char>& raw_buffer) 
                : _ptr(reinterpret_cast<const T*>(raw_buffer.data())),
                _size(raw_buffer.size() / sizeof(T)) {}

            // Access element like vector
            const T& operator[](size_t index) const { return _ptr[index]; }
            const T& at(size_t index) const {
                if (index >= _size) throw std::out_of_range("biomxt::Cells: Index out of range");
                return _ptr[index];
            }

            // Iterator support (so that we can use for(auto x : cells))
            const T* begin() const { return _ptr; }
            const T* end() const { return _ptr + _size; }

            // Basic information
            size_t size() const { return _size; }
            bool empty() const { return _size == 0; }
            const T* data() const { return _ptr; }

        private:
            const T* _ptr;
            size_t _size;
    };
} // namespace biomxt
