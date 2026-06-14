#include "cli/Commands.h"

#include "common/Formatting.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>

namespace maya_rfid {

int configCheck(const AppConfig& config) {
    std::cout << "config ok\n";
    std::cout << "device: " << config.deviceId << '\n';
    std::cout << "reader: " << config.readerId << '\n';
    std::cout << "reader_host: " << config.readerHost << ':' << config.readerPort << '\n';
    if (!config.readerSourceHost.empty()) {
        std::cout << "reader_source_host: " << config.readerSourceHost << '\n';
    }
    std::cout << "fallback_url: " << config.fixedFallbackUrl << '\n';
    return 0;
}

int storageCheck(const AppConfig& config) {
    std::filesystem::create_directories(config.eventJsonlPath.parent_path());
    std::filesystem::create_directories(config.sqlitePath.parent_path());
    std::ofstream(config.eventJsonlPath, std::ios::app).close();
    std::cout << "storage ok\n";
    std::cout << "event_jsonl: " << config.eventJsonlPath << '\n';
    std::cout << "sqlite_placeholder: " << config.sqlitePath << '\n';
    return 0;
}

int tokenStatus(const AppConfig&) {
    std::cout << "available_tokens: fixed-fallback\n";
    return 0;
}

int syncPeek(const AppConfig&) {
    std::cout << "pending_sync: 0\n";
    return 0;
}

int syncAck(const AppConfig&) {
    std::cout << "sync acked: no queued rows in this addon example\n";
    return 0;
}

int syncFail(const AppConfig&, std::string_view errorMessage) {
    std::cout << "sync failure recorded locally: " << (errorMessage.empty() ? "manual sync failure" : errorMessage) << '\n';
    return 0;
}

int tokenAdd(const AppConfig&, std::string_view token, std::string_view url) {
    if (token.empty() || url.empty()) {
        std::cerr << "token-add requires --token and --url\n";
        return 2;
    }
    std::cout << "token accepted for external/cloud queue: " << token << " -> " << url << '\n';
    return 0;
}

int buildNdef(std::string_view url, int maxUserBytes) {
    auto built = tcx::nfc::NdefUriBuilder::buildHttpsUriTlv(url, static_cast<size_t>(maxUserBytes));
    if (!built.ok) {
        std::cerr << built.error << '\n';
        return 1;
    }
    std::cout << "tlv_bytes: ";
    printBytes(built.value.tlvBytes);
    std::cout << "padded_bytes: ";
    printBytes(built.value.paddedBytes);
    std::cout << "pages: " << built.value.pageCount << '\n';
    return 0;
}

int readerPing(const AppConfig& config) {
    tcx::nfc::TcpSocketTransport transport(endpointFor(config));
    auto ok = transport.connectCheck();
    if (!ok.ok) {
        std::cerr << ok.error << '\n';
        return 1;
    }
    std::cout << "reader online\n";
    return 0;
}

int readUid(const AppConfig& config) {
    const auto endpoint = endpointFor(config);
    tcx::nfc::TcpSocketTransport privateTcp(endpoint);
    tcx::nfc::TcpSocketTransport modbusTcp(endpoint);
    tcx::nfc::Bks710iReader reader(privateTcp, modbusTcp);
    auto uid = reader.readUid();
    if (!uid.ok) {
        std::cerr << uid.error << '\n';
        return 1;
    }
    std::cout << "uid: " << uid.value.uidHex << '\n';
    return 0;
}

int writeUrl(const AppConfig& config, std::string_view url) {
    const auto endpoint = endpointFor(config);
    tcx::nfc::TcpSocketTransport privateTcp(endpoint);
    tcx::nfc::TcpSocketTransport modbusTcp(endpoint);
    tcx::nfc::Bks710iReader reader(privateTcp, modbusTcp);
    auto result = reader.writeUrlRawNtag(url.empty() ? config.fixedFallbackUrl : std::string(url), config.ntagMaxUserBytes, config.ntagStartPage);
    if (!result.ok) {
        std::cerr << result.error << '\n';
        return 1;
    }
    std::cout << "write_strategy: " << result.value.writeStrategy << '\n';
    std::cout << "verification: " << result.value.verificationLevel << '\n';
    std::cout << "pages: " << result.value.pagesWritten << '\n';
    return result.value.verificationLevel == "verified" ? 0 : 2;
}

int readNdef(const AppConfig& config, int startPage, int endPage) {
    if (endPage < startPage) {
        std::cerr << "end page must be >= start page\n";
        return 2;
    }

    tcx::nfc::TcpSocketTransport transport(endpointFor(config));
    tcx::nfc::ModbusTcpClient client(transport);
    auto trigger = client.writeMultipleRegisters(tcx::nfc::Bks710iModbus::kTriggerRegisterAddress, tcx::nfc::Bks710iModbus::buildFastReadRegisters(static_cast<uint16_t>(startPage), static_cast<uint16_t>(endPage)));
    if (!trigger.ok) {
        std::cerr << trigger.error << '\n';
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    const int pageCount = endPage - startPage + 1;
    const uint16_t headerBytes = 4;
    const uint16_t bytesToRead = static_cast<uint16_t>(headerBytes + pageCount * 4);
    const uint16_t registersToRead = static_cast<uint16_t>((bytesToRead + 1) / 2);
    auto registers = client.readHoldingRegisters(tcx::nfc::Bks710iModbus::kResultRegisterAddress, registersToRead);
    if (!registers.ok) {
        std::cerr << registers.error << '\n';
        return 1;
    }

    auto bytes = tcx::nfc::Bks710iModbus::registersToBytes(registers.value);
    std::cout << "raw_readback: ";
    printBytes(bytes);
    if (bytes.size() > headerBytes) {
        std::vector<uint8_t> ntagBytes(bytes.begin() + headerBytes, bytes.end());
        std::cout << "ntag_pages: ";
        printBytes(ntagBytes);
    }
    return 0;
}

} // namespace maya_rfid
