#include "tcxcef/LocalAssetServer.h"

#include "impl/httplib.h"

#include <sstream>

namespace tcxCEF {

LocalAssetServer::LocalAssetServer() = default;

LocalAssetServer::~LocalAssetServer() {
    stop();
}

bool LocalAssetServer::start(const LocalAssetServerSettings& settings) {
    stop();

    if (settings.root.empty() || !std::filesystem::is_directory(settings.root)) {
        return false;
    }

    auto server = std::make_unique<httplib::Server>();
    const auto absoluteRoot = std::filesystem::absolute(settings.root);
    if (!server->set_mount_point("/", absoluteRoot.string())) {
        return false;
    }

    int boundPort = 0;
    if (settings.preferredPort > 0) {
        if (!server->bind_to_port(settings.host, settings.preferredPort)) {
            return false;
        }
        boundPort = settings.preferredPort;
    } else {
        boundPort = server->bind_to_any_port(settings.host);
        if (boundPort <= 0) {
            return false;
        }
    }

    server_ = std::move(server);
    root_ = absoluteRoot;
    host_ = settings.host;
    port_ = boundPort;
    running_ = true;

    serverThread_ = std::thread([this]() {
        server_->listen_after_bind();
        running_ = false;
    });

    return true;
}

void LocalAssetServer::stop() {
    if (server_) {
        server_->stop();
    }
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    server_.reset();
    running_ = false;
    port_ = 0;
}

bool LocalAssetServer::isRunning() const {
    return running_;
}

int LocalAssetServer::port() const {
    return port_;
}

const std::filesystem::path& LocalAssetServer::root() const {
    return root_;
}

std::string LocalAssetServer::url(const std::string& path) const {
    std::ostringstream out;
    out << "http://" << host_ << ":" << port_;
    if (path.empty() || path.front() != '/') {
        out << "/";
    }
    out << path;
    return out.str();
}

} // namespace tcxCEF
