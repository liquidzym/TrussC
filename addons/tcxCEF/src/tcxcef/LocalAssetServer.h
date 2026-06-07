#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

namespace httplib {
class Server;
}

namespace tcxCEF {

struct LocalAssetServerSettings {
    std::filesystem::path root;
    std::string host = "127.0.0.1";
    int preferredPort = 0;
};

class LocalAssetServer {
public:
    LocalAssetServer();
    ~LocalAssetServer();

    LocalAssetServer(const LocalAssetServer&) = delete;
    LocalAssetServer& operator=(const LocalAssetServer&) = delete;

    bool start(const LocalAssetServerSettings& settings);
    void stop();

    bool isRunning() const;
    int port() const;
    const std::filesystem::path& root() const;
    std::string url(const std::string& path = "/") const;

private:
    std::unique_ptr<httplib::Server> server_;
    std::thread serverThread_;
    std::filesystem::path root_;
    std::string host_ = "127.0.0.1";
    int port_ = 0;
    std::atomic<bool> running_{false};
};

} // namespace tcxCEF
