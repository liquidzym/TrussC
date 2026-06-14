#include "tcx/nfc/Common.h"

#include <iomanip>
#include <sstream>

namespace tcx::nfc {

std::string formatUidHex(const std::vector<uint8_t>& uidBytes) {
    std::ostringstream out;
    for (size_t i = 0; i < uidBytes.size(); ++i) {
        if (i != 0) {
            out << ':';
        }
        out << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(uidBytes[i]);
    }
    return out.str();
}

} // namespace tcx::nfc
