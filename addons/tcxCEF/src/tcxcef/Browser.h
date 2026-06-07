#pragma once

#include <memory>
#include <string>

namespace tcxCEF {

struct BrowserSettings {
    std::string url;
    bool showWindow = true;
    bool openDevTools = false;
    int width = 960;
    int height = 720;
};

class Browser {
public:
    Browser();
    ~Browser();

    Browser(const Browser&) = delete;
    Browser& operator=(const Browser&) = delete;

    bool setup(const BrowserSettings& settings);
    void update();
    void shutdown();

    bool isReady() const;
    bool isAvailable() const;
    std::string lastError() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace tcxCEF
