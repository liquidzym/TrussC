#include <tcxNFC.h>

#include <iomanip>
#include <iostream>
#include <string>

namespace {

void printBytes(const std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i != 0) {
            std::cout << ' ';
        }
        std::cout << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
    }
    std::cout << std::dec << '\n';
}

} // namespace

int main(int argc, char** argv) {
    const std::string url = argc > 1 ? argv[1] : "https://wstree.cn/t/ABC123";
    auto ndef = tcx::nfc::NdefUriBuilder::buildHttpsUriTlv(url, 144);
    if (!ndef.ok) {
        std::cerr << ndef.error << '\n';
        return 1;
    }

    std::cout << "TLV bytes:\n";
    printBytes(ndef.value.tlvBytes);
    std::cout << "Padded bytes (" << ndef.value.pageCount << " pages):\n";
    printBytes(ndef.value.paddedBytes);
    return 0;
}
