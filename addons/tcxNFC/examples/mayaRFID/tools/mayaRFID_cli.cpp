#include "MayaRFIDRuntime.h"

#include <iostream>

int main(int argc, char** argv) {
    const auto options = maya_rfid::parseOptions(argc, argv);
    if (options.command.empty()) {
        maya_rfid::printCliUsage();
        return 2;
    }

    const auto configPath = maya_rfid::resolveConfigPath(options.configPath, argv[0]);
    auto config = maya_rfid::loadConfig(configPath);
    if (!config.ok) {
        std::cerr << config.error << '\n';
        return 1;
    }

    if (!options.host.empty()) {
        config.value.readerHost = options.host;
    }
    if (!options.sourceHost.empty()) {
        config.value.readerSourceHost = options.sourceHost;
    }

    const auto& command = options.command;
    if (command == "config-check") {
        return maya_rfid::configCheck(config.value);
    }
    if (command == "storage-check") {
        return maya_rfid::storageCheck(config.value);
    }
    if (command == "token-status") {
        return maya_rfid::tokenStatus(config.value);
    }
    if (command == "sync-peek") {
        return maya_rfid::syncPeek(config.value);
    }
    if (command == "sync-ack") {
        return maya_rfid::syncAck(config.value);
    }
    if (command == "sync-fail") {
        return maya_rfid::syncFail(config.value, options.errorMessage);
    }
    if (command == "token-add") {
        return maya_rfid::tokenAdd(config.value, options.token, options.url);
    }
    if (command == "build-ndef") {
        return maya_rfid::buildNdef(options.url.empty() ? config.value.fixedFallbackUrl : options.url, config.value.ntagMaxUserBytes);
    }
    if (command == "reader-ping") {
        return maya_rfid::readerPing(config.value);
    }
    if (command == "read-uid") {
        return maya_rfid::readUid(config.value);
    }
    if (command == "write-url") {
        return maya_rfid::writeUrl(config.value, options.url);
    }
    if (command == "read-ndef") {
        return maya_rfid::readNdef(config.value, options.startPage, options.endPage);
    }
    if (command == "mock-once") {
        return maya_rfid::runActivationOnce(config.value, true);
    }

    std::cerr << "unknown command: " << command << '\n';
    maya_rfid::printCliUsage();
    return 2;
}
