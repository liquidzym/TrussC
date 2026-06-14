#include "tcx/nfc/Reader.h"

#include "tcx/nfc/Ndef.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <memory>
#include <thread>
#include <utility>

#ifndef _WIN32
#include <arpa/inet.h>
#include <cerrno>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace tcx::nfc {
namespace {

uint16_t readU16(const std::vector<uint8_t>& bytes, size_t offset) {
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[offset]) << 8U) | bytes[offset + 1]);
}

#ifndef _WIN32
class SocketHandle {
public:
    explicit SocketHandle(int fd = -1)
        : fd_(fd) {}

    ~SocketHandle() {
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    SocketHandle(const SocketHandle&) = delete;
    SocketHandle& operator=(const SocketHandle&) = delete;

    int get() const {
        return fd_;
    }

    int release() {
        const int fd = fd_;
        fd_ = -1;
        return fd;
    }

private:
    int fd_ = -1;
};

Result<int> connectSocket(const TcpEndpoint& endpoint) {
    addrinfo hints {};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;

    addrinfo* rawRemote = nullptr;
    const std::string port = std::to_string(endpoint.port);
    const int lookup = getaddrinfo(endpoint.host.c_str(), port.c_str(), &hints, &rawRemote);
    if (lookup != 0) {
        return Result<int>::failure("resolve reader host failed: " + std::string(gai_strerror(lookup)));
    }

    std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> remote(rawRemote, freeaddrinfo);
    std::string lastError = "no address candidates";
    for (addrinfo* addr = remote.get(); addr != nullptr; addr = addr->ai_next) {
        SocketHandle socketHandle(socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol));
        if (socketHandle.get() < 0) {
            lastError = std::strerror(errno);
            continue;
        }

        if (!endpoint.sourceHost.empty()) {
            addrinfo sourceHints {};
            sourceHints.ai_socktype = SOCK_STREAM;
            sourceHints.ai_family = addr->ai_family;
            addrinfo* rawSource = nullptr;
            const int sourceLookup = getaddrinfo(endpoint.sourceHost.c_str(), "0", &sourceHints, &rawSource);
            if (sourceLookup == 0) {
                std::unique_ptr<addrinfo, decltype(&freeaddrinfo)> source(rawSource, freeaddrinfo);
                if (bind(socketHandle.get(), source->ai_addr, source->ai_addrlen) != 0) {
                    lastError = "bind source host failed: " + std::string(std::strerror(errno));
                    continue;
                }
            } else {
                lastError = "resolve source host failed: " + std::string(gai_strerror(sourceLookup));
                continue;
            }
        }

        if (endpoint.timeoutMs > 0) {
            timeval tv {};
            tv.tv_sec = endpoint.timeoutMs / 1000;
            tv.tv_usec = (endpoint.timeoutMs % 1000) * 1000;
            setsockopt(socketHandle.get(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(socketHandle.get(), SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        }

        if (connect(socketHandle.get(), addr->ai_addr, addr->ai_addrlen) == 0) {
            return Result<int>::success(socketHandle.release());
        }
        lastError = std::strerror(errno);
    }

    return Result<int>::failure("connect reader failed: " + lastError);
}
#endif

} // namespace

TcpSocketTransport::TcpSocketTransport(TcpEndpoint endpoint)
    : endpoint_(std::move(endpoint)) {}

Result<bool> TcpSocketTransport::connectCheck() {
#ifdef _WIN32
    return Result<bool>::failure("TcpSocketTransport is not implemented on Windows yet");
#else
    auto fd = connectSocket(endpoint_);
    if (!fd.ok) {
        return Result<bool>::failure(fd.error);
    }
    SocketHandle socketHandle(fd.value);
    return Result<bool>::success(true);
#endif
}

Result<std::vector<uint8_t>> TcpSocketTransport::transact(const std::vector<uint8_t>& request, size_t minResponseBytes) {
#ifdef _WIN32
    (void)request;
    (void)minResponseBytes;
    return Result<std::vector<uint8_t>>::failure("TcpSocketTransport is not implemented on Windows yet");
#else
    auto fd = connectSocket(endpoint_);
    if (!fd.ok) {
        return Result<std::vector<uint8_t>>::failure(fd.error);
    }
    SocketHandle socketHandle(fd.value);

    size_t sent = 0;
    while (sent < request.size()) {
        const ssize_t n = send(socketHandle.get(), request.data() + sent, request.size() - sent, 0);
        if (n <= 0) {
            return Result<std::vector<uint8_t>>::failure("send reader request failed: " + std::string(std::strerror(errno)));
        }
        sent += static_cast<size_t>(n);
    }

    std::vector<uint8_t> response;
    response.reserve(std::max<size_t>(minResponseBytes, 64));
    std::vector<uint8_t> buffer(256);
    while (response.size() < minResponseBytes) {
        const ssize_t n = recv(socketHandle.get(), buffer.data(), buffer.size(), 0);
        if (n < 0) {
            return Result<std::vector<uint8_t>>::failure("receive reader response failed: " + std::string(std::strerror(errno)));
        }
        if (n == 0) {
            break;
        }
        response.insert(response.end(), buffer.begin(), buffer.begin() + n);
    }

    if (response.size() < minResponseBytes) {
        return Result<std::vector<uint8_t>>::failure("reader response shorter than expected");
    }
    return Result<std::vector<uint8_t>>::success(std::move(response));
#endif
}

ModbusTcpClient::ModbusTcpClient(IByteTransport& transport, uint8_t unitId)
    : transport_(transport)
    , unitId_(unitId) {}

Result<ModbusWriteResult> ModbusTcpClient::writeMultipleRegisters(uint16_t startAddress, const std::vector<uint16_t>& registers) {
    const uint16_t transactionId = nextTransactionId_++;
    auto request = Bks710iModbus::buildWriteMultipleRegistersRequest(transactionId, unitId_, startAddress, registers);
    auto response = transport_.transact(request, 12);
    if (!response.ok) {
        return Result<ModbusWriteResult>::failure(response.error);
    }

    const auto& bytes = response.value;
    if (bytes.size() < 12 || readU16(bytes, 0) != transactionId || bytes[6] != unitId_ || bytes[7] != 0x10) {
        return Result<ModbusWriteResult>::failure("invalid Modbus 0x10 response");
    }

    ModbusWriteResult result;
    result.startAddress = readU16(bytes, 8);
    result.registerCount = readU16(bytes, 10);
    return Result<ModbusWriteResult>::success(result);
}

Result<std::vector<uint16_t>> ModbusTcpClient::readHoldingRegisters(uint16_t startAddress, uint16_t registerCount) {
    const uint16_t transactionId = nextTransactionId_++;
    auto request = Bks710iModbus::buildReadHoldingRegistersRequest(transactionId, unitId_, startAddress, registerCount);
    auto response = transport_.transact(request, 9);
    if (!response.ok) {
        return Result<std::vector<uint16_t>>::failure(response.error);
    }
    return Bks710iModbus::parseReadHoldingRegistersResponse(response.value, transactionId, unitId_);
}

PrivateProtocolClient::PrivateProtocolClient(IByteTransport& transport)
    : transport_(transport) {}

Result<CardUid> PrivateProtocolClient::readIso14443aUid(uint8_t beepLedHint, uint8_t controlByte) {
    auto request = PrivateProtocolFrames::buildReadIso14443aUidRequest(beepLedHint, controlByte);
    auto response = transport_.transact(request, 13);
    if (!response.ok) {
        return Result<CardUid>::failure(response.error);
    }

    const auto& bytes = response.value;
    if (bytes.size() < 13 || bytes[0] != 0xA5 || bytes[1] != 0x5A || bytes[10] != 0x50) {
        return Result<CardUid>::failure("invalid private 0x50 response frame");
    }
    if (bytes[11] != 0x00) {
        return Result<CardUid>::failure("private 0x50 response status failure");
    }

    const size_t checksumIndex = bytes.size() - 1;
    if (PrivateProtocolFrames::checksum({bytes.data(), checksumIndex}) != bytes[checksumIndex]) {
        return Result<CardUid>::failure("private 0x50 response checksum mismatch");
    }

    std::vector<uint8_t> data(bytes.begin() + 12, bytes.end() - 1);
    if (data.empty()) {
        return Result<CardUid>::failure("private 0x50 response has no UID data");
    }

    CardUid uid;
    uid.cardType = data[0];
    if (data.size() >= 2 && static_cast<size_t>(data[1]) <= data.size() - 2) {
        uid.uidBytes.assign(data.begin() + 2, data.begin() + 2 + data[1]);
    } else {
        uid.uidBytes.assign(data.begin() + 1, data.end());
    }
    if (uid.uidBytes.empty()) {
        return Result<CardUid>::failure("private 0x50 response UID is empty");
    }
    uid.uidHex = formatUidHex(uid.uidBytes);
    return Result<CardUid>::success(std::move(uid));
}

Bks710iReader::Bks710iReader(IByteTransport& privateTransport, IByteTransport& modbusTransport)
    : privateTransport_(privateTransport)
    , modbusTransport_(modbusTransport) {}

Result<bool> Bks710iReader::ping() {
    return modbusTransport_.connectCheck();
}

Result<CardUid> Bks710iReader::readUid(uint8_t beepLedHint, uint8_t controlByte) {
    PrivateProtocolClient client(privateTransport_);
    auto privateUid = client.readIso14443aUid(beepLedHint, controlByte);
    if (privateUid.ok) {
        return privateUid;
    }

    auto modbusUid = readUidModbus();
    if (modbusUid.ok) {
        return modbusUid;
    }

    return Result<CardUid>::failure("private UID failed: " + privateUid.error + "; modbus UID failed: " + modbusUid.error);
}

Result<CardUid> Bks710iReader::readUidModbus() {
    ModbusTcpClient client(modbusTransport_);
    auto trigger = client.writeMultipleRegisters(Bks710iModbus::kTriggerRegisterAddress, {0x5022, 0x0000});
    if (!trigger.ok) {
        return Result<CardUid>::failure(trigger.error);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    auto registers = client.readHoldingRegisters(Bks710iModbus::kResultRegisterAddress, 5);
    if (!registers.ok) {
        return Result<CardUid>::failure(registers.error);
    }
    if (registers.value.empty()) {
        return Result<CardUid>::failure("Modbus UID result is empty");
    }

    const uint16_t statusAndCommand = registers.value[0];
    const uint8_t status = static_cast<uint8_t>((statusAndCommand >> 8U) & 0xFFU);
    const uint8_t command = static_cast<uint8_t>(statusAndCommand & 0xFFU);
    if (status != 0x01 || command != 0x50) {
        return Result<CardUid>::failure("Modbus 0x50 UID command did not report success");
    }

    std::vector<uint16_t> dataRegisters(registers.value.begin() + 1, registers.value.end());
    auto data = Bks710iModbus::registersToBytes(dataRegisters);
    while (!data.empty() && data.back() == 0x00 && data.size() > 5) {
        data.pop_back();
    }
    if (data.size() < 2) {
        return Result<CardUid>::failure("Modbus 0x50 UID data too short");
    }

    CardUid uid;
    uid.cardType = data[0];
    uid.uidBytes.assign(data.begin() + 1, data.end());
    uid.uidHex = formatUidHex(uid.uidBytes);
    return Result<CardUid>::success(std::move(uid));
}

Result<WriteResult> Bks710iReader::writeUrlRawNtag(const std::string& url, int maxUserBytes, int startPage) {
    auto ndef = NdefUriBuilder::buildHttpsUriTlv(url, static_cast<size_t>(maxUserBytes));
    if (!ndef.ok) {
        return Result<WriteResult>::failure(ndef.error);
    }

    auto registers = Bks710iModbus::buildNtagWriteRegisters(static_cast<uint16_t>(startPage), ndef.value.paddedBytes);
    ModbusTcpClient client(modbusTransport_);
    auto write = client.writeMultipleRegisters(Bks710iModbus::kTriggerRegisterAddress, registers);
    if (!write.ok) {
        return Result<WriteResult>::failure(write.error);
    }

    auto verified = verifyRawNtagWrite(client, ndef.value.paddedBytes, startPage, ndef.value.pageCount);
    if (!verified.ok) {
        return Result<WriteResult>::failure(verified.error);
    }

    WriteResult result;
    result.writeStrategy = "raw_0x62";
    result.verificationLevel = verified.value ? "verified" : "readback_mismatch";
    result.pagesWritten = ndef.value.pageCount;
    return Result<WriteResult>::success(std::move(result));
}

Result<bool> Bks710iReader::verifyRawNtagWrite(ModbusTcpClient& client, const std::vector<uint8_t>& expectedPaddedBytes, int startPage, int pageCount) {
    const int endPage = startPage + pageCount - 1;
    auto trigger = client.writeMultipleRegisters(Bks710iModbus::kTriggerRegisterAddress, Bks710iModbus::buildFastReadRegisters(static_cast<uint16_t>(startPage), static_cast<uint16_t>(endPage)));
    if (!trigger.ok) {
        return Result<bool>::failure("readback trigger failed: " + trigger.error);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    const uint16_t headerBytes = 4;
    const uint16_t bytesToRead = static_cast<uint16_t>(headerBytes + expectedPaddedBytes.size());
    const uint16_t registersToRead = static_cast<uint16_t>((bytesToRead + 1) / 2);
    auto registers = client.readHoldingRegisters(Bks710iModbus::kResultRegisterAddress, registersToRead);
    if (!registers.ok) {
        return Result<bool>::failure("readback result failed: " + registers.error);
    }

    auto bytes = Bks710iModbus::registersToBytes(registers.value);
    if (bytes.size() < headerBytes + expectedPaddedBytes.size()) {
        return Result<bool>::failure("readback result shorter than expected NDEF bytes");
    }

    const auto begin = bytes.begin() + headerBytes;
    const auto end = begin + static_cast<std::ptrdiff_t>(expectedPaddedBytes.size());
    return Result<bool>::success(std::equal(begin, end, expectedPaddedBytes.begin(), expectedPaddedBytes.end()));
}

} // namespace tcx::nfc
