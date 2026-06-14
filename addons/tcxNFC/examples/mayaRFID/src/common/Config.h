#pragma once

#include <tcxNFC.h>

#include <filesystem>
#include <string>

namespace maya_rfid {

struct AppConfig {
    std::string deviceId = "pi-reader-01";
    std::string readerId = "tree-root-01";
    std::string locationName;
    std::string siteId;
    int preventDuplicateWriteSec = 10;
    std::string readerHost = "192.168.1.100";
    std::string readerSourceHost;
    int readerPort = 502;
    int ntagStartPage = 4;
    int ntagMaxUserBytes = 144;
    std::string fixedFallbackUrl = "https://www.baidu.com/";
    std::filesystem::path sqlitePath = "bin/data/worldtree_rfid.db";
    std::filesystem::path eventJsonlPath = "bin/data/logs/events.jsonl";
};

struct CliOptions {
    std::filesystem::path configPath;
    std::string command;
    std::string mode;
    std::string url;
    std::string token;
    std::string host;
    std::string sourceHost;
    std::string errorMessage;
    int loopCount = 0;
    int startPage = 4;
    int endPage = 39;
    bool once = false;
    bool mock = false;
};

CliOptions parseOptions(int argc, char** argv);
std::filesystem::path resolveConfigPath(const std::filesystem::path& requested, const char* argv0);
tcx::nfc::Result<AppConfig> loadConfig(const std::filesystem::path& path);
tcx::nfc::TcpEndpoint endpointFor(const AppConfig& config);

void printMainUsage();
void printCliUsage();

} // namespace maya_rfid
