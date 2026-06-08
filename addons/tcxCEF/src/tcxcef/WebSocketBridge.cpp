#include "tcxcef/WebSocketBridge.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#include <utility>

namespace tcxCEF {
namespace {

constexpr const char* kWebSocketMagic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
constexpr uint64_t kMaxTextFrameBytes = 8ull * 1024ull * 1024ull;

std::string base64Encode(const std::uint8_t* data, size_t size) {
    static constexpr char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((size + 2) / 3) * 4);

    for (size_t i = 0; i < size; i += 3) {
        const uint32_t b0 = data[i];
        const uint32_t b1 = (i + 1 < size) ? data[i + 1] : 0;
        const uint32_t b2 = (i + 2 < size) ? data[i + 2] : 0;
        const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;

        out.push_back(chars[(triple >> 18) & 0x3F]);
        out.push_back(chars[(triple >> 12) & 0x3F]);
        out.push_back((i + 1 < size) ? chars[(triple >> 6) & 0x3F] : '=');
        out.push_back((i + 2 < size) ? chars[triple & 0x3F] : '=');
    }
    return out;
}

uint32_t rotateLeft(uint32_t value, uint32_t bits) {
    return (value << bits) | (value >> (32 - bits));
}

std::array<std::uint8_t, 20> sha1(const std::string& input) {
    uint32_t state[5] = {
        0x67452301u,
        0xEFCDAB89u,
        0x98BADCFEu,
        0x10325476u,
        0xC3D2E1F0u,
    };

    std::vector<std::uint8_t> buffer(input.begin(), input.end());
    const uint64_t bitLength = static_cast<uint64_t>(buffer.size()) * 8;
    buffer.push_back(0x80);
    while ((buffer.size() + 8) % 64 != 0) {
        buffer.push_back(0);
    }
    for (int i = 7; i >= 0; --i) {
        buffer.push_back(static_cast<std::uint8_t>((bitLength >> (i * 8)) & 0xFF));
    }

    for (size_t offset = 0; offset < buffer.size(); offset += 64) {
        uint32_t words[80] = {};
        for (int i = 0; i < 16; ++i) {
            const size_t j = offset + static_cast<size_t>(i) * 4;
            words[i] = (static_cast<uint32_t>(buffer[j]) << 24) |
                       (static_cast<uint32_t>(buffer[j + 1]) << 16) |
                       (static_cast<uint32_t>(buffer[j + 2]) << 8) |
                       static_cast<uint32_t>(buffer[j + 3]);
        }
        for (int i = 16; i < 80; ++i) {
            words[i] = rotateLeft(words[i - 3] ^ words[i - 8] ^ words[i - 14] ^ words[i - 16], 1);
        }

        uint32_t a = state[0];
        uint32_t b = state[1];
        uint32_t c = state[2];
        uint32_t d = state[3];
        uint32_t e = state[4];

        for (int i = 0; i < 80; ++i) {
            uint32_t f = 0;
            uint32_t k = 0;
            if (i < 20) {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999u;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1u;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDCu;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6u;
            }

            const uint32_t temp = rotateLeft(a, 5) + f + e + k + words[i];
            e = d;
            d = c;
            c = rotateLeft(b, 30);
            b = a;
            a = temp;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
    }

    std::array<std::uint8_t, 20> digest{};
    for (int i = 0; i < 5; ++i) {
        digest[static_cast<size_t>(i) * 4] = static_cast<std::uint8_t>((state[i] >> 24) & 0xFF);
        digest[static_cast<size_t>(i) * 4 + 1] = static_cast<std::uint8_t>((state[i] >> 16) & 0xFF);
        digest[static_cast<size_t>(i) * 4 + 2] = static_cast<std::uint8_t>((state[i] >> 8) & 0xFF);
        digest[static_cast<size_t>(i) * 4 + 3] = static_cast<std::uint8_t>(state[i] & 0xFF);
    }
    return digest;
}

std::string trimHeaderValue(std::string value) {
    while (!value.empty() && (value.back() == '\r' || value.back() == '\n' || value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }
    size_t first = value.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return {};
    }
    return value.substr(first);
}

std::string findHeader(const std::string& request, const std::string& name) {
    std::istringstream in(request);
    std::string line;
    const std::string prefix = name + ":";
    while (std::getline(in, line)) {
        if (line.size() >= prefix.size()) {
            const std::string head = line.substr(0, prefix.size());
            if (std::equal(head.begin(), head.end(), prefix.begin(), prefix.end(),
                           [](char a, char b) {
                               return std::tolower(static_cast<unsigned char>(a)) ==
                                      std::tolower(static_cast<unsigned char>(b));
                           })) {
                return trimHeaderValue(line.substr(prefix.size()));
            }
        }
    }
    return {};
}

} // namespace

WebSocketBridge::WebSocketBridge() {
    connectListener_ = server_.onClientConnect.listen(this, &WebSocketBridge::handleConnect);
    receiveListener_ = server_.onReceive.listen(this, &WebSocketBridge::handleReceive);
    disconnectListener_ = server_.onClientDisconnect.listen(this, &WebSocketBridge::handleDisconnect);
    errorListener_ = server_.onError.listen(this, &WebSocketBridge::handleError);
}

WebSocketBridge::~WebSocketBridge() {
    stop();
}

bool WebSocketBridge::start(const WebSocketBridgeSettings& settings) {
    stop();
    settings_ = settings;

    if (settings_.preferredPort > 0) {
        return tryStartPort(settings_.preferredPort);
    }

    for (int candidate = 14540; candidate < 14640; ++candidate) {
        if (tryStartPort(candidate)) {
            return true;
        }
    }
    std::string error = "No available WebSocketBridge port in range 14540-14639";
    onError.notify(error);
    return false;
}

void WebSocketBridge::stop() {
    server_.stop();
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.clear();
}

bool WebSocketBridge::isRunning() const {
    return server_.isRunning();
}

int WebSocketBridge::port() const {
    return server_.getPort();
}

std::string WebSocketBridge::url() const {
    std::ostringstream out;
    out << "ws://" << settings_.host << ":" << port() << settings_.path;
    return out.str();
}

bool WebSocketBridge::send(int clientId, const std::string& text) {
    return sendTextFrame(clientId, text);
}

void WebSocketBridge::broadcast(const std::string& text) {
    const auto clientIds = server_.getClientIds();
    for (int clientId : clientIds) {
        sendTextFrame(clientId, text);
    }
}

bool WebSocketBridge::tryStartPort(int port) {
    return server_.start(port, settings_.maxClients);
}

void WebSocketBridge::handleConnect(tc::TcpClientConnectEventArgs& args) {
    std::lock_guard<std::mutex> lock(mutex_);
    clients_[args.clientId] = ClientState{};
}

void WebSocketBridge::handleReceive(tc::TcpServerReceiveEventArgs& args) {
    std::vector<WebSocketBridgeMessage> messages;
    bool disconnectClient = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = clients_.find(args.clientId);
        if (it == clients_.end()) {
            return;
        }

        auto& state = it->second;
        state.buffer.insert(state.buffer.end(), args.data.begin(), args.data.end());
        if (!state.handshaken) {
            if (!processHandshake(args.clientId, state)) {
                return;
            }
        }
        processFrames(args.clientId, state, messages, disconnectClient);
    }

    for (auto& message : messages) {
        onMessage.notify(message);
    }
    if (disconnectClient) {
        server_.disconnectClient(args.clientId);
    }
}

void WebSocketBridge::handleDisconnect(tc::TcpClientDisconnectEventArgs& args) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_.erase(args.clientId);
    }
    onClientDisconnected.notify(args.clientId);
}

void WebSocketBridge::handleError(tc::TcpServerErrorEventArgs& args) {
    onError.notify(args.message);
}

bool WebSocketBridge::processHandshake(int clientId, ClientState& state) {
    const std::string request(state.buffer.begin(), state.buffer.end());
    const size_t headerEnd = request.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return false;
    }

    std::istringstream requestLines(request.substr(0, headerEnd));
    std::string method;
    std::string path;
    requestLines >> method >> path;
    if (method != "GET" || path != settings_.path) {
        const std::string response = "HTTP/1.1 404 Not Found\r\n\r\n";
        server_.send(clientId, response);
        server_.disconnectClient(clientId);
        return false;
    }

    const std::string key = findHeader(request.substr(0, headerEnd), "Sec-WebSocket-Key");
    if (key.empty()) {
        const std::string response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        server_.send(clientId, response);
        server_.disconnectClient(clientId);
        return false;
    }

    const auto digest = sha1(key + kWebSocketMagic);
    const std::string accept = base64Encode(digest.data(), digest.size());
    const std::string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
    server_.send(clientId, response);

    state.buffer.erase(state.buffer.begin(), state.buffer.begin() + static_cast<long>(headerEnd + 4));
    state.handshaken = true;
    onClientConnected.notify(clientId);
    return true;
}

void WebSocketBridge::processFrames(int clientId,
                                    ClientState& state,
                                    std::vector<WebSocketBridgeMessage>& messages,
                                    bool& disconnectClient) {
    auto failProtocol = [&]() {
        state.buffer.clear();
        state.hasFragmentedMessage = false;
        state.fragmentedOpcode = 0;
        state.fragmentedPayload.clear();
        disconnectClient = true;
    };

    auto pushTextMessage = [&](std::string text) {
        WebSocketBridgeMessage message;
        message.clientId = clientId;
        message.text = std::move(text);
        messages.push_back(std::move(message));
    };

    while (state.buffer.size() >= 2) {
        const std::uint8_t byte0 = state.buffer[0];
        const std::uint8_t byte1 = state.buffer[1];
        const bool fin = (byte0 & 0x80) != 0;
        const bool reservedBits = (byte0 & 0x70) != 0;
        const std::uint8_t opcode = byte0 & 0x0F;
        const bool controlFrame = opcode >= 0x8;
        const bool masked = (byte1 & 0x80) != 0;
        uint64_t payloadLength = byte1 & 0x7F;
        size_t offset = 2;

        if (reservedBits) {
            failProtocol();
            return;
        }

        if (payloadLength == 126) {
            if (state.buffer.size() < offset + 2) {
                return;
            }
            payloadLength = (static_cast<uint64_t>(state.buffer[offset]) << 8) |
                            static_cast<uint64_t>(state.buffer[offset + 1]);
            offset += 2;
        } else if (payloadLength == 127) {
            if (state.buffer.size() < offset + 8) {
                return;
            }
            payloadLength = 0;
            for (int i = 0; i < 8; ++i) {
                payloadLength = (payloadLength << 8) | state.buffer[offset + static_cast<size_t>(i)];
            }
            offset += 8;
        }

        if (controlFrame && (!fin || payloadLength > 125)) {
            failProtocol();
            return;
        }

        if (payloadLength > kMaxTextFrameBytes) {
            failProtocol();
            return;
        }

        std::array<std::uint8_t, 4> maskKey{};
        if (masked) {
            if (state.buffer.size() < offset + maskKey.size()) {
                return;
            }
            std::copy_n(state.buffer.begin() + static_cast<long>(offset), maskKey.size(), maskKey.begin());
            offset += maskKey.size();
        }

        if (state.buffer.size() < offset + payloadLength) {
            return;
        }

        std::string payload;
        payload.resize(static_cast<size_t>(payloadLength));
        for (size_t i = 0; i < payload.size(); ++i) {
            std::uint8_t value = state.buffer[offset + i];
            if (masked) {
                value ^= maskKey[i % maskKey.size()];
            }
            payload[i] = static_cast<char>(value);
        }

        state.buffer.erase(state.buffer.begin(),
                           state.buffer.begin() + static_cast<long>(offset + payloadLength));

        if (opcode == 0x1) {
            if (state.hasFragmentedMessage) {
                failProtocol();
                return;
            }
            if (fin) {
                pushTextMessage(std::move(payload));
            } else {
                state.hasFragmentedMessage = true;
                state.fragmentedOpcode = opcode;
                state.fragmentedPayload = std::move(payload);
            }
        } else if (opcode == 0x0) {
            if (!state.hasFragmentedMessage) {
                failProtocol();
                return;
            }
            if (state.fragmentedPayload.size() > kMaxTextFrameBytes - payload.size()) {
                failProtocol();
                return;
            }
            state.fragmentedPayload.append(payload);
            if (fin) {
                if (state.fragmentedOpcode == 0x1) {
                    pushTextMessage(std::move(state.fragmentedPayload));
                }
                state.hasFragmentedMessage = false;
                state.fragmentedOpcode = 0;
                state.fragmentedPayload.clear();
            }
        } else if (opcode == 0x8) {
            state.hasFragmentedMessage = false;
            state.fragmentedOpcode = 0;
            state.fragmentedPayload.clear();
            disconnectClient = true;
            return;
        } else if (opcode == 0x9) {
            sendControlFrame(clientId, 0xA, payload);
        } else if (opcode == 0xA) {
            continue;
        } else {
            failProtocol();
            return;
        }
    }
}

bool WebSocketBridge::sendTextFrame(int clientId, const std::string& text) {
    std::vector<char> frame;
    frame.push_back(static_cast<char>(0x81));

    if (text.size() < 126) {
        frame.push_back(static_cast<char>(text.size()));
    } else if (text.size() <= 0xFFFF) {
        frame.push_back(static_cast<char>(126));
        frame.push_back(static_cast<char>((text.size() >> 8) & 0xFF));
        frame.push_back(static_cast<char>(text.size() & 0xFF));
    } else {
        frame.push_back(static_cast<char>(127));
        const uint64_t length = static_cast<uint64_t>(text.size());
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<char>((length >> (i * 8)) & 0xFF));
        }
    }

    frame.insert(frame.end(), text.begin(), text.end());
    return server_.send(clientId, frame);
}

bool WebSocketBridge::sendControlFrame(int clientId, std::uint8_t opcode, const std::string& payload) {
    if (payload.size() > 125) {
        return false;
    }

    std::vector<char> frame;
    frame.reserve(2 + payload.size());
    frame.push_back(static_cast<char>(0x80 | (opcode & 0x0F)));
    frame.push_back(static_cast<char>(payload.size()));
    frame.insert(frame.end(), payload.begin(), payload.end());
    return server_.send(clientId, frame);
}

} // namespace tcxCEF
