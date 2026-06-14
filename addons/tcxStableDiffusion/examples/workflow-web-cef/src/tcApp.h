#pragma once

#include "NodeWorkerProcess.h"

#include <TrussC.h>
#include <tcxCEF.h>

#include <filesystem>
#include <string>

class tcApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void cleanup() override;

private:
    std::filesystem::path executableDir() const;
    std::filesystem::path webRoot() const;
    std::filesystem::path nodeExecutable() const;
    std::filesystem::path workerScript() const;
    void startWorker();
    void resizeBrowserToWindow();
    void handleBridgeMessage(tcxCEF::WebSocketBridgeMessage& message);
    void sendStatus(const std::string& stage, const std::string& detail);
    std::string workerUnavailableJson() const;

    tcxCEF::LocalAssetServer assetServer_;
    tcxCEF::WebSocketBridge bridge_;
    tcxCEF::Browser browser_;
    tc::EventListener bridgeMessageListener_;
    NodeWorkerProcess worker_;
    int browserWidth_ = 0;
    int browserHeight_ = 0;
    std::string status_ = "Starting";
    std::string lastError_;
};
