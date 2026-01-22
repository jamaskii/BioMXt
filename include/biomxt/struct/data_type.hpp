#pragma once
#include <cstdint>
#include <iostream>


namespace biomxt {
    /**
     * @brief Biomxt data type enum.
     */
    enum DataType : uint8_t {
        UNKNOWN = 0,
        INT16 = 1,
        INT32 = 2,
        INT64 = 3,
        FLOAT32 = 4,
        FLOAT64 = 5
    };

    /**
     * @brief Convert data type enum to string.
     * @param dtype Data type enum.
     * @return `std::string` String representation of data type.
     */
    inline std::string dtype_to_string(DataType dtype) {
        switch (dtype) {
            case INT16: return "int16";
            case INT32: return "int32";
            case INT64: return "int64";
            case FLOAT32: return "float32";
            case FLOAT64: return "float64";
            default: return "unknown";
        }
    }

    /**
     * @brief Convert string to data type enum.
     * @param type String representation of data type.
     * @return `DataType` Data type enum.
     */
    inline DataType dtype_from_string(const std::string& type) {
        if (type == "int16") return DataType::INT16;
        if (type == "int32") return DataType::INT32;
        if (type == "int64") return DataType::INT64;
        if (type == "float32" || type == "float") return DataType::FLOAT32;
        if (type == "float64" || type == "double") return DataType::FLOAT64;
        return DataType::UNKNOWN;
    }

    /**
     * @brief Get size of data type in bytes.
     * @param dtype Data type enum.
     * @return `uint32_t` Size of data type in bytes.
     */
    inline uint32_t size_of_dtype(DataType dtype) {
        switch (dtype) {
            case INT16: return sizeof(int16_t);
            case INT32: return sizeof(int32_t);
            case INT64: return sizeof(int64_t);
            case FLOAT32: return sizeof(float);
            case FLOAT64: return sizeof(double);
            default: return 0;
        }
    }

    /**
     * @brief Get data type enum from type.
     * @tparam `T` Type.
     * @note example: `dtype_from_type<int32_t>::value` is `DataType::INT32`, `dtype_from_type<std::string>::valid` is `false`
     */
    template<typename T>
               struct dtype_from_type          { static constexpr DataType value = DataType::UNKNOWN; static constexpr bool valid = false; };
    template<> struct dtype_from_type<int16_t> { static constexpr DataType value = DataType::INT16; static constexpr bool valid = true; };
    template<> struct dtype_from_type<int32_t> { static constexpr DataType value = DataType::INT32; static constexpr bool valid = true; };
    template<> struct dtype_from_type<int64_t> { static constexpr DataType value = DataType::INT64; static constexpr bool valid = true; };
    template<> struct dtype_from_type<float>   { static constexpr DataType value = DataType::FLOAT32; static constexpr bool valid = true; };
    template<> struct dtype_from_type<double>  { static constexpr DataType value = DataType::FLOAT64; static constexpr bool valid = true; };

    /**
     * @brief Get type from data type enum.
     * @tparam `DataType` DType Data type enum.
     */
    template <DataType DType>
                struct type_from_dtype;
    template <> struct type_from_dtype<DataType::INT16>   { using type = int16_t; };
    template <> struct type_from_dtype<DataType::INT32>   { using type = int32_t; };
    template <> struct type_from_dtype<DataType::INT64>   { using type = int64_t; };
    template <> struct type_from_dtype<DataType::FLOAT32> { using type = float; };
    template <> struct type_from_dtype<DataType::FLOAT64> { using type = double; };


}
