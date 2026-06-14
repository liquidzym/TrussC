#include "common/Formatting.h"

#include <iomanip>
#include <iostream>

namespace maya_rfid {

void printBytes(const std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
    }
    std::cout << std::dec << '\n';
}

} // namespace maya_rfid
