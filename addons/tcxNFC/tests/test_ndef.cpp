#include "test_common.h"

void test_ndef_https_uri_tlv() {
    auto built = tcx::nfc::NdefUriBuilder::buildHttpsUriTlv("https://wstree.cn/t/ABC123", 144);
    require(built.ok, built.error.c_str());

    const std::vector<uint8_t> expected {
        0x03, 0x17, 0xD1, 0x01, 0x13, 0x55, 0x04,
        'w', 's', 't', 'r', 'e', 'e', '.', 'c', 'n',
        '/', 't', '/', 'A', 'B', 'C', '1', '2', '3',
        0xFE
    };
    requireEqual(built.value.tlvBytes, expected, "NDEF URI TLV bytes should match Type 2 Tag URI record");
    require(built.value.pageCount == 7, "NDEF URI should pad to seven NTAG pages");
    require(built.value.paddedBytes.size() == 28, "NDEF URI should pad to a 4-byte page boundary");
    require(built.value.paddedBytes[26] == 0x00 && built.value.paddedBytes[27] == 0x00, "NDEF padding should use zero bytes");

    auto badScheme = tcx::nfc::NdefUriBuilder::buildHttpsUriTlv("http://wstree.cn/t/ABC123", 144);
    require(!badScheme.ok, "NDEF URI builder should reject non-HTTPS URLs");

    auto tooLarge = tcx::nfc::NdefUriBuilder::buildHttpsUriTlv("https://wstree.cn/t/ABC123", 8);
    require(!tooLarge.ok, "NDEF URI builder should reject URLs beyond NTAG capacity");
}
