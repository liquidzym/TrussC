#pragma once

#include "common/Config.h"
#include "runtime/ActivationRuntime.h"

#include <TrussC.h>

#include <filesystem>
#include <future>
#include <string>

namespace maya_rfid {

class MayaRFIDGuiApp : public tc::App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    void reloadConfig();
    void startActivation(bool mock);
    void finishPending();
    void setStatus(std::string status);

    AppConfig config_;
    std::filesystem::path configPath_;
    ActivationSummary lastSummary_;
    std::future<tcx::nfc::Result<ActivationSummary>> pending_;
    std::string status_ = "Loading config";
    bool configLoaded_ = false;
    bool hasPending_ = false;
    int runCount_ = 0;
};

} // namespace maya_rfid
