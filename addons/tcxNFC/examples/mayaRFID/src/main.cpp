#include "MayaRFIDRuntime.h"

#include <iostream>

int main(int argc, char** argv) {
    const auto options = maya_rfid::parseOptions(argc, argv);
    const auto configPath = maya_rfid::resolveConfigPath(options.configPath, argv[0]);
    auto config = maya_rfid::loadConfig(configPath);
    if (!config.ok) {
        std::cerr << config.error << '\n';
        return 1;
    }

    std::string mode = options.mode.empty() ? "headless" : options.mode;
    if (mode == "mock") {
        return maya_rfid::runActivationOnce(config.value, true);
    }

    if (mode == "gui") {
        std::cerr << "GUI is now built as the separate mayaRFID_gui target.\n";
        return 2;
    }

    if (mode == "headless") {
        if (options.once) {
            return maya_rfid::runActivationOnce(config.value, options.mock);
        }
        return maya_rfid::runActivationLoop(config.value, options.loopCount, options.mock);
    }

    std::cerr << "unknown mode: " << mode << '\n';
    maya_rfid::printMainUsage();
    return 2;
}
