#pragma once

#include "tc/events/tcEvent.h"
#include "tc/events/tcEventListener.h"
#include "tc/network/tcTcpServer.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace tcxCEF {

struct WebSocketBridgeSettings {
    std::string host = "127.0.0.1";
    int preferredPort = 0;
    int maxClients = 4;
    std::string path = "/bridge";
};

struct WebSocketBridgeMessage {
    int clientId = -1;
    std::string text;
};

class WebSocketBridge {
public:
    tc::Event<int> onClientConnected;
    tc::Event<int> onClientDisconnected;
    tc::Event<WebSocketBridgeMessage> onMessage;
    tc::Event<std::string> onError;

    WebSocketBridge();
    ~WebSocketBridge();

    WebSocketBridge(const WebSocketBridge&) = delete;
    WebSocketBridge& operator=(const WebSocketBridge&) = delete;

    bool start(const WebSocketBridgeSettings& settings = {});
    void stop();
    bool isRunning() const;
    int port() const;
    std::string url() const;

    bool send(int clientId, const std::string& text);
    void broadcast(const std::string& text);

private:
    struct ClientState {
        bool handshaken = false;
        std::vector<std::uint8_t> buffer;
    };

    bool tryStartPort(int port);
    void handleConnect(tc::TcpClientConnectEventArgs& args);
    void handleReceive(tc::TcpServerReceiveEventArgs& args);
    void handleDisconnect(tc::TcpClientDisconnectEventArgs& args);
    void handleError(tc::TcpServerErrorEventArgs& args);

    bool processHandshake(int clientId, ClientState& state);
    void processFrames(int clientId,
                       ClientState& state,
                       std::vector<WebSocketBridgeMessage>& messages,
                       bool& disconnectClient);
    bool sendTextFrame(int clientId, const std::string& text);

    tc::TcpServer server_;
    tc::EventListener connectListener_;
    tc::EventListener receiveListener_;
    tc::EventListener disconnectListener_;
    tc::EventListener errorListener_;
    WebSocketBridgeSettings settings_;
    std::map<int, ClientState> clients_;
    mutable std::mutex mutex_;
};

} // namespace tcxCEF
