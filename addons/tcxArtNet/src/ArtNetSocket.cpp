#include "tcx/artnet/ArtNetSocket.h"

#include <array>
#include <cstring>
#include <string>

#if defined(_WIN32)
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace tcx::artnet {

namespace {

#if defined(_WIN32)
using SocketHandle = SOCKET;
constexpr SocketHandle InvalidSocket = INVALID_SOCKET;

std::string socketErrorMessage() {
    return "Winsock error " + std::to_string(WSAGetLastError());
}

bool wouldBlock() {
    const int error = WSAGetLastError();
    return error == WSAEWOULDBLOCK || error == WSAEINTR;
}

class WinsockRuntime {
public:
    WinsockRuntime() {
        WSADATA data {};
        ok_ = WSAStartup(MAKEWORD(2, 2), &data) == 0;
    }

    ~WinsockRuntime() {
        if (ok_) {
            WSACleanup();
        }
    }

    bool ok() const noexcept { return ok_; }

private:
    bool ok_ = false;
};

WinsockRuntime& winsockRuntime() {
    static WinsockRuntime runtime;
    return runtime;
}

void closeNative(SocketHandle handle) {
    closesocket(handle);
}
#else
using SocketHandle = int;
constexpr SocketHandle InvalidSocket = -1;

std::string socketErrorMessage() {
    return std::strerror(errno);
}

bool wouldBlock() {
    return errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR;
}

void closeNative(SocketHandle handle) {
    ::close(handle);
}
#endif

bool fillAddress(const Endpoint& endpoint, sockaddr_in& address, Error* error) {
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(endpoint.port);
    if (endpoint.ip.empty()) {
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        return true;
    }
    if (inet_pton(AF_INET, endpoint.ip.c_str(), &address.sin_addr) != 1) {
        setError(error, ErrorCode::InvalidAddress, "invalid IPv4 endpoint: " + endpoint.ip);
        return false;
    }
    return true;
}

} // namespace

struct UdpSocket::Impl {
    SocketHandle handle = InvalidSocket;
    bool nonBlocking = false;
    bool reuseAddress = false;
    bool broadcast = false;
    std::string requestedBindIp = "0.0.0.0";
    uint16_t requestedBindPort = 0;
    Endpoint actualLocalEndpoint;
    Error lastError;
};

namespace {

void recordError(Error& lastError, Error* error, ErrorCode code, const std::string& message) {
    lastError = Error { code, message };
    setError(error, code, message);
}

void clearError(Error& lastError) {
    lastError = {};
}

Endpoint queryLocalEndpoint(SocketHandle handle) {
    Endpoint endpoint;
    if (handle == InvalidSocket) {
        return endpoint;
    }

    sockaddr_in local {};
#if defined(_WIN32)
    int localSize = sizeof(local);
#else
    socklen_t localSize = sizeof(local);
#endif
    if (getsockname(handle, reinterpret_cast<sockaddr*>(&local), &localSize) != 0) {
        return endpoint;
    }

    std::array<char, INET_ADDRSTRLEN> ipBuffer {};
    if (!inet_ntop(AF_INET, &local.sin_addr, ipBuffer.data(), static_cast<socklen_t>(ipBuffer.size()))) {
        return endpoint;
    }
    endpoint.ip = ipBuffer.data();
    endpoint.port = ntohs(local.sin_port);
    return endpoint;
}

} // namespace

UdpSocket::UdpSocket()
    : impl_(std::make_unique<Impl>()) {}

UdpSocket::~UdpSocket() {
    close();
}

UdpSocket::UdpSocket(UdpSocket&&) noexcept = default;
UdpSocket& UdpSocket::operator=(UdpSocket&&) noexcept = default;

bool UdpSocket::open(Error* error) {
#if defined(_WIN32)
    if (!winsockRuntime().ok()) {
        recordError(impl_->lastError, error, ErrorCode::SocketError, "WSAStartup failed");
        return false;
    }
#endif
    if (isOpen()) {
        return true;
    }
    impl_->handle = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (impl_->handle == InvalidSocket) {
        recordError(impl_->lastError, error, ErrorCode::SocketError, socketErrorMessage());
        return false;
    }
    clearError(impl_->lastError);
    return true;
}

bool UdpSocket::bind(uint16_t port, Error* error) {
    return bind("", port, error);
}

bool UdpSocket::bind(const std::string& localIp, uint16_t port, Error* error) {
    if (!open(error)) {
        return false;
    }
    sockaddr_in address {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (localIp.empty() || localIp == "0.0.0.0") {
        address.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (inet_pton(AF_INET, localIp.c_str(), &address.sin_addr) != 1) {
        recordError(impl_->lastError, error, ErrorCode::InvalidAddress, "invalid local bind IPv4 endpoint: " + localIp);
        return false;
    }
    if (::bind(impl_->handle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        recordError(impl_->lastError, error, ErrorCode::BindFailed, socketErrorMessage());
        return false;
    }
    impl_->requestedBindIp = (localIp.empty() || localIp == "0.0.0.0") ? "0.0.0.0" : localIp;
    impl_->requestedBindPort = port;
    impl_->actualLocalEndpoint = queryLocalEndpoint(impl_->handle);
    clearError(impl_->lastError);
    return true;
}

bool UdpSocket::close() {
    if (!impl_ || impl_->handle == InvalidSocket) {
        return true;
    }
    closeNative(impl_->handle);
    impl_->handle = InvalidSocket;
    impl_->nonBlocking = false;
    impl_->actualLocalEndpoint = {};
    return true;
}

bool UdpSocket::setNonBlocking(bool enabled, Error* error) {
    if (!open(error)) {
        return false;
    }
#if defined(_WIN32)
    u_long mode = enabled ? 1UL : 0UL;
    if (ioctlsocket(impl_->handle, FIONBIO, &mode) != 0) {
        recordError(impl_->lastError, error, ErrorCode::SocketError, socketErrorMessage());
        return false;
    }
#else
    const int flags = fcntl(impl_->handle, F_GETFL, 0);
    if (flags < 0) {
        recordError(impl_->lastError, error, ErrorCode::SocketError, socketErrorMessage());
        return false;
    }
    const int nextFlags = enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (fcntl(impl_->handle, F_SETFL, nextFlags) != 0) {
        recordError(impl_->lastError, error, ErrorCode::SocketError, socketErrorMessage());
        return false;
    }
#endif
    impl_->nonBlocking = enabled;
    clearError(impl_->lastError);
    return true;
}

bool UdpSocket::setReuseAddress(bool enabled, Error* error) {
    if (!open(error)) {
        return false;
    }
    int value = enabled ? 1 : 0;
    if (setsockopt(impl_->handle, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&value), sizeof(value)) != 0) {
        recordError(impl_->lastError, error, ErrorCode::SocketError, socketErrorMessage());
        return false;
    }
    impl_->reuseAddress = enabled;
    clearError(impl_->lastError);
    return true;
}

bool UdpSocket::setBroadcast(bool enabled, Error* error) {
    if (!open(error)) {
        return false;
    }
    int value = enabled ? 1 : 0;
    if (setsockopt(impl_->handle, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&value), sizeof(value)) != 0) {
        recordError(impl_->lastError, error, ErrorCode::SocketError, socketErrorMessage());
        return false;
    }
    impl_->broadcast = enabled;
    clearError(impl_->lastError);
    return true;
}

bool UdpSocket::sendTo(std::span<const uint8_t> data, const Endpoint& endpoint, Error* error) {
    if (!open(error)) {
        return false;
    }
    sockaddr_in address {};
    Error addressError;
    if (!fillAddress(endpoint, address, &addressError)) {
        recordError(impl_->lastError, error, addressError.code, addressError.message);
        return false;
    }
    const int sent = ::sendto(
        impl_->handle,
        reinterpret_cast<const char*>(data.data()),
        static_cast<int>(data.size()),
        0,
        reinterpret_cast<sockaddr*>(&address),
        sizeof(address)
    );
    if (sent < 0 || static_cast<size_t>(sent) != data.size()) {
        recordError(impl_->lastError, error, ErrorCode::SendFailed, socketErrorMessage());
        return false;
    }
    impl_->actualLocalEndpoint = queryLocalEndpoint(impl_->handle);
    clearError(impl_->lastError);
    return true;
}

bool UdpSocket::receiveFrom(std::span<uint8_t> buffer, size_t& bytesReceived, Endpoint& sender, Error* error) {
    bytesReceived = 0;
    if (!isOpen()) {
        recordError(impl_->lastError, error, ErrorCode::NotOpen, "UDP socket is not open");
        return false;
    }
    sockaddr_in remote {};
#if defined(_WIN32)
    int remoteSize = sizeof(remote);
#else
    socklen_t remoteSize = sizeof(remote);
#endif
    const int received = ::recvfrom(
        impl_->handle,
        reinterpret_cast<char*>(buffer.data()),
        static_cast<int>(buffer.size()),
        0,
        reinterpret_cast<sockaddr*>(&remote),
        &remoteSize
    );
    if (received < 0) {
        if (wouldBlock()) {
            setError(error, ErrorCode::None, "no UDP packet available");
        } else {
            recordError(impl_->lastError, error, ErrorCode::ReceiveFailed, socketErrorMessage());
        }
        return false;
    }

    std::array<char, INET_ADDRSTRLEN> ipBuffer {};
    inet_ntop(AF_INET, &remote.sin_addr, ipBuffer.data(), static_cast<socklen_t>(ipBuffer.size()));
    sender.ip = ipBuffer.data();
    sender.port = ntohs(remote.sin_port);
    bytesReceived = static_cast<size_t>(received);
    return true;
}

bool UdpSocket::isOpen() const noexcept {
    return impl_ && impl_->handle != InvalidSocket;
}

SocketDiagnostics UdpSocket::diagnostics() const {
    SocketDiagnostics diagnostics;
    if (!impl_) {
        return diagnostics;
    }
    diagnostics.open = isOpen();
    diagnostics.reuseAddress = impl_->reuseAddress;
    diagnostics.broadcast = impl_->broadcast;
    diagnostics.nonBlocking = impl_->nonBlocking;
    diagnostics.requestedBindIp = impl_->requestedBindIp;
    diagnostics.requestedBindPort = impl_->requestedBindPort;
    diagnostics.actualLocalEndpoint = diagnostics.open ? queryLocalEndpoint(impl_->handle) : impl_->actualLocalEndpoint;
    diagnostics.lastError = impl_->lastError;
    return diagnostics;
}

} // namespace tcx::artnet
