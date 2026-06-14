#pragma once

#include "common/Config.h"

#include <tcxNFC.h>

#include <string>

namespace maya_rfid {

struct ActivationSummary {
    std::string uid;
    std::string token;
    std::string url;
    std::string urlMode;
    std::string writeStrategy;
    std::string verificationLevel;
    int pagesWritten = 0;
};

tcx::nfc::Result<ActivationSummary> activateOnce(const AppConfig& config, bool mock);
int runActivationOnce(const AppConfig& config, bool mock);
int runActivationLoop(const AppConfig& config, int loopCount, bool mock);

} // namespace maya_rfid
