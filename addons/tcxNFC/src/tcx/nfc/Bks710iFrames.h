#pragma once

#include "tcx/nfc/Common.h"

#include <cstdint>
#include <initializer_list>
#include <span>
#include <vector>

namespace tcx::nfc {

class Bks710iModbus {
public:
    static constexpr uint16_t kTriggerRegisterAddress = 0x0000;
    static constexpr uint16_t kResultRegisterAddress = 0x0095;
    static constexpr uint8_t kDefaultUnitId = 0x01;

    static std::vector<uint16_t> buildNtagWriteRegisters(uint16_t startPage, std::initializer_list<uint8_t> data);
    static std::vector<uint16_t> buildNtagWriteRegisters(uint16_t startPage, const std::vector<uint8_t>& data);
    static std::vector<uint16_t> buildFastReadRegisters(uint16_t startPage, uint16_t endPage);
    static std::vector<uint8_t> registersToBytes(const std::vector<uint16_t>& registers);
    static std::vector<uint8_t> buildWriteMultipleRegistersRequest(uint16_t transactionId, uint8_t unitId, uint16_t startAddress, std::initializer_list<uint16_t> registers);
    static std::vector<uint8_t> buildWriteMultipleRegistersRequest(uint16_t transactionId, uint8_t unitId, uint16_t startAddress, const std::vector<uint16_t>& registers);
    static std::vector<uint8_t> buildWriteMultipleRegistersResponse(uint16_t transactionId, uint8_t unitId, uint16_t startAddress, uint16_t registerCount);
    static std::vector<uint16_t> extractWriteMultipleRegisterValues(const std::vector<uint8_t>& request);
    static std::vector<uint8_t> buildReadHoldingRegistersRequest(uint16_t transactionId, uint8_t unitId, uint16_t startAddress, uint16_t registerCount);
    static Result<std::vector<uint16_t>> parseReadHoldingRegistersResponse(const std::vector<uint8_t>& response, uint16_t transactionId, uint8_t unitId);
};

class PrivateProtocolFrames {
public:
    static std::vector<uint8_t> buildReadIso14443aUidRequest(uint8_t beepLedHint = 0x22, uint8_t controlByte = 0x00);
    static uint8_t checksum(std::span<const uint8_t> bytesWithoutChecksum);
};

} // namespace tcx::nfc
