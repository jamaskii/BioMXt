#pragma once
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <cstring>


namespace biomxt {
    struct UUID {
        uint8_t data[16];

        // Randomly generate a UUID (Version 4)
        static UUID generate() {
            UUID uuid;
            // Use hardware random number generator as seed
            std::random_device rd;
            std::mt19937_64 gen(rd());
            std::uniform_int_distribution<uint64_t> dis;

            uint64_t* ptr = reinterpret_cast<uint64_t*>(uuid.data);
            ptr[0] = dis(gen);
            ptr[1] = dis(gen);

            uuid.data[6] = (uuid.data[6] & 0x0f) | 0x40;
            uuid.data[8] = (uuid.data[8] & 0x3f) | 0x80;

            return uuid;
        }

        std::string to_string() const {
            std::stringstream ss;
            ss << std::hex << std::setfill('0');
            for (int i = 0; i < 16; ++i) {
                ss << std::setw(2) << static_cast<int>(data[i]);
                if (i == 3 || i == 5 || i == 7 || i == 9) ss << "-";
            }
            return ss.str();
        }

        bool operator==(const UUID& other) const {
            return std::memcmp(data, other.data, 16) == 0;
        }
    };
}