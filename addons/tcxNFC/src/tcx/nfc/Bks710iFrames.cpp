#include "tcx/nfc/Bks710iFrames.h"

namespace tcx::nfc {
namespace {

void pushU16(std::vector<uint8_t>& bytes, uint16_t value) {
    bytes.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
    bytes.push_back(static_cast<uint8_t>(value & 0xFFU));
}

uint16_t readU16(const std::vector<uint8_t>& bytes, size_t offset) {
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[offset]) << 8U) | bytes[offset + 1]);
}

} // namespace

std::vector<uint16_t> Bks710iModbus::buildNtagWriteRegisters(uint16_t startPage, std::initializer_list<uint8_t> data) {
    return buildNtagWriteRegisters(startPage, std::vector<uint8_t>(data));
}

std::vector<uint16_t> Bks710iModbus::buildNtagWriteRegisters(uint16_t startPage, const std::vector<uint8_t>& data) {
    std::vector<uint16_t> registers {
        0x6222,
        0x0000,
        startPage,
    };

    for (size_t i = 0; i < data.size(); i += 2) {
        const uint16_t high = static_cast<uint16_t>(data[i]) << 8U;
        const uint16_t low = (i + 1 < data.size()) ? data[i + 1] : 0x00;
        registers.push_back(static_cast<uint16_t>(high | low));
    }

    return registers;
}

std::vector<uint16_t> Bks710iModbus::buildFastReadRegisters(uint16_t startPage, uint16_t endPage) {
    return {0x6122, 0x0000, startPage, endPage};
}

std::vector<uint8_t> Bks710iModbus::registersToBytes(const std::vector<uint16_t>& registers) {
    std::vector<uint8_t> bytes;
    bytes.reserve(registers.size() * 2);
    for (const auto reg : registers) {
        bytes.push_back(static_cast<uint8_t>((reg >> 8U) & 0xFFU));
        bytes.push_back(static_cast<uint8_t>(reg & 0xFFU));
    }
    return bytes;
}

std::vector<uint8_t> Bks710iModbus::buildWriteMultipleRegistersRequest(uint16_t transactionId, uint8_t unitId, uint16_t startAddress, std::initializer_list<uint16_t> registers) {
    return buildWriteMultipleRegistersRequest(transactionId, unitId, startAddress, std::vector<uint16_t>(registers));
}

std::vector<uint8_t> Bks710iModbus::buildWriteMultipleRegistersRequest(uint16_t transactionId, uint8_t unitId, uint16_t startAddress, const std::vector<uint16_t>& registers) {
    std::vector<uint8_t> request;
    const uint16_t registerCount = static_cast<uint16_t>(registers.size());
    const uint8_t byteCount = static_cast<uint8_t>(registerCount * 2);
    const uint16_t pduLength = static_cast<uint16_t>(1 + 2 + 2 + 1 + byteCount);
    const uint16_t mbapLength = static_cast<uint16_t>(1 + pduLength);

    pushU16(request, transactionId);
    pushU16(request, 0x0000);
    pushU16(request, mbapLength);
    request.push_back(unitId);
    request.push_back(0x10);
    pushU16(request, startAddress);
    pushU16(request, registerCount);
    request.push_back(byteCount);
    for (const auto reg : registers) {
        pushU16(request, reg);
    }
    return request;
}

std::vector<uint8_t> Bks710iModbus::buildWriteMultipleRegistersResponse(uint16_t transactionId, uint8_t unitId, uint16_t startAddress, uint16_t registerCount) {
    std::vector<uint8_t> response;
    pushU16(response, transactionId);
    pushU16(response, 0x0000);
    pushU16(response, 0x0006);
    response.push_back(unitId);
    response.push_back(0x10);
    pushU16(response, startAddress);
    pushU16(response, registerCount);
    return response;
}

std::vector<uint16_t> Bks710iModbus::extractWriteMultipleRegisterValues(const std::vector<uint8_t>& request) {
    if (request.size() < 13 || request[7] != 0x10) {
        return {};
    }

    const uint16_t registerCount = readU16(request, 10);
    std::vector<uint16_t> registers;
    registers.reserve(registerCount);
    size_t offset = 13;
    for (uint16_t i = 0; i < registerCount && offset + 1 < request.size(); ++i, offset += 2) {
        registers.push_back(readU16(request, offset));
    }
    return registers;
}

std::vector<uint8_t> Bks710iModbus::buildReadHoldingRegistersRequest(uint16_t transactionId, uint8_t unitId, uint16_t startAddress, uint16_t registerCount) {
    std::vector<uint8_t> request;
    pushU16(request, transactionId);
    pushU16(request, 0x0000);
    pushU16(request, 0x0006);
    request.push_back(unitId);
    request.push_back(0x03);
    pushU16(request, startAddress);
    pushU16(request, registerCount);
    return request;
}

Result<std::vector<uint16_t>> Bks710iModbus::parseReadHoldingRegistersResponse(const std::vector<uint8_t>& response, uint16_t transactionId, uint8_t unitId) {
    if (response.size() < 9) {
        return Result<std::vector<uint16_t>>::failure("Modbus read response too short");
    }
    if (readU16(response, 0) != transactionId || response[6] != unitId || response[7] != 0x03) {
        return Result<std::vector<uint16_t>>::failure("invalid Modbus 0x03 response header");
    }

    const uint8_t byteCount = response[8];
    if (byteCount % 2 != 0 || response.size() < static_cast<size_t>(9 + byteCount)) {
        return Result<std::vector<uint16_t>>::failure("invalid Modbus 0x03 byte count");
    }

    std::vector<uint16_t> registers;
    registers.reserve(byteCount / 2);
    for (size_t offset = 9; offset < static_cast<size_t>(9 + byteCount); offset += 2) {
        registers.push_back(readU16(response, offset));
    }
    return Result<std::vector<uint16_t>>::success(std::move(registers));
}

std::vector<uint8_t> PrivateProtocolFrames::buildReadIso14443aUidRequest(uint8_t beepLedHint, uint8_t controlByte) {
    std::vector<uint8_t> frame {
        0xA5, 0x5A,
        0x01, 0x01,
        0x00, 0x0B,
        0x50,
        0x00,
        beepLedHint,
        controlByte,
    };
    frame.push_back(checksum(frame));
    return frame;
}

uint8_t PrivateProtocolFrames::checksum(std::span<const uint8_t> bytesWithoutChecksum) {
    uint8_t value = 0;
    for (const auto byte : bytesWithoutChecksum) {
        value = static_cast<uint8_t>(value + byte);
    }
    return value;
}

} // namespace tcx::nfc
