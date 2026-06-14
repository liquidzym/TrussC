#pragma once

#include "tcx/nfc/Activation.h"
#include "tcx/nfc/Bks710iFrames.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tcx::nfc {

struct TcpEndpoint {
    std::string host = "192.168.1.100";
    uint16_t port = 502;
    std::string sourceHost;
    int timeoutMs = 1000;
};

class IByteTransport {
public:
    virtual ~IByteTransport() = default;
    virtual Result<std::vector<uint8_t>> transact(const std::vector<uint8_t>& request, size_t minResponseBytes) = 0;
    virtual Result<bool> connectCheck() = 0;
};

class TcpSocketTransport final : public IByteTransport {
public:
    explicit TcpSocketTransport(TcpEndpoint endpoint);

    Result<std::vector<uint8_t>> transact(const std::vector<uint8_t>& request, size_t minResponseBytes) override;
    Result<bool> connectCheck() override;

private:
    TcpEndpoint endpoint_;
};

struct ModbusWriteResult {
    uint16_t startAddress = 0;
    uint16_t registerCount = 0;
};

class ModbusTcpClient {
public:
    explicit ModbusTcpClient(IByteTransport& transport, uint8_t unitId = Bks710iModbus::kDefaultUnitId);

    Result<ModbusWriteResult> writeMultipleRegisters(uint16_t startAddress, const std::vector<uint16_t>& registers);
    Result<std::vector<uint16_t>> readHoldingRegisters(uint16_t startAddress, uint16_t registerCount);

private:
    IByteTransport& transport_;
    uint8_t unitId_ = Bks710iModbus::kDefaultUnitId;
    uint16_t nextTransactionId_ = 1;
};

class PrivateProtocolClient {
public:
    explicit PrivateProtocolClient(IByteTransport& transport);

    Result<CardUid> readIso14443aUid(uint8_t beepLedHint = 0x22, uint8_t controlByte = 0x00);

private:
    IByteTransport& transport_;
};

class Bks710iReader final : public IReader {
public:
    Bks710iReader(IByteTransport& privateTransport, IByteTransport& modbusTransport);

    Result<bool> ping();
    Result<CardUid> readUid(uint8_t beepLedHint = 0x22, uint8_t controlByte = 0x00) override;
    Result<WriteResult> writeUrlRawNtag(const std::string& url, int maxUserBytes, int startPage) override;

private:
    Result<CardUid> readUidModbus();
    Result<bool> verifyRawNtagWrite(ModbusTcpClient& client, const std::vector<uint8_t>& expectedPaddedBytes, int startPage, int pageCount);

    IByteTransport& privateTransport_;
    IByteTransport& modbusTransport_;
};

} // namespace tcx::nfc
