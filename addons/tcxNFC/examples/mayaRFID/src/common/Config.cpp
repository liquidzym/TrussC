#include "common/Config.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

namespace maya_rfid {
namespace {

std::string trim(std::string value) {
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char c) { return !isSpace(static_cast<unsigned char>(c)); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char c) { return !isSpace(static_cast<unsigned char>(c)); }).base(), value.end());
    return value;
}

std::string stripComment(std::string value) {
    bool inQuote = false;
    char quote = 0;
    for (size_t i = 0; i < value.size(); ++i) {
        const char c = value[i];
        if ((c == '"' || c == '\'') && (i == 0 || value[i - 1] != '\\')) {
            if (!inQuote) {
                inQuote = true;
                quote = c;
            } else if (quote == c) {
                inQuote = false;
                quote = 0;
            }
        }
        if (c == '#' && !inQuote) {
            value.resize(i);
            break;
        }
    }
    return trim(value);
}

std::string unquote(std::string value) {
    value = trim(value);
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\''))) {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

std::map<std::string, std::string> parseSimpleYaml(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::map<std::string, std::string> values;
    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        line = stripComment(line);
        if (line.empty()) {
            continue;
        }

        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            continue;
        }

        auto key = trim(line.substr(0, colon));
        auto value = trim(line.substr(colon + 1));
        if (value.empty()) {
            section = key;
            continue;
        }

        if (!section.empty()) {
            key = section + "." + key;
        }
        values[key] = unquote(value);
    }
    return values;
}

std::string stringValue(const std::map<std::string, std::string>& values, const std::string& key, std::string fallback) {
    const auto it = values.find(key);
    return it == values.end() ? fallback : it->second;
}

int intValue(const std::map<std::string, std::string>& values, const std::string& key, int fallback) {
    const auto it = values.find(key);
    if (it == values.end() || it->second.empty()) {
        return fallback;
    }
    return std::stoi(it->second);
}

std::filesystem::path sourceDir() {
#ifdef MAYARFID_EXAMPLE_SOURCE_DIR
    return std::filesystem::path(MAYARFID_EXAMPLE_SOURCE_DIR);
#else
    return std::filesystem::current_path();
#endif
}

std::filesystem::path executableDir(const char* argv0) {
    if (argv0 == nullptr || std::string(argv0).empty()) {
        return std::filesystem::current_path();
    }
    std::filesystem::path path(argv0);
    if (path.is_relative()) {
        path = std::filesystem::current_path() / path;
    }
    return path.parent_path();
}

std::filesystem::path asConfigFile(const std::filesystem::path& path) {
    if (std::filesystem::is_directory(path)) {
        return path / "pi01.example.yaml";
    }
    return path;
}

std::string optionValue(int argc, char** argv, const std::string& name, std::string fallback = {}) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == name) {
            return argv[i + 1];
        }
    }
    return fallback;
}

bool hasFlag(int argc, char** argv, const std::string& name) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == name) {
            return true;
        }
    }
    return false;
}

std::string commandName(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--config" || arg == "--url" || arg == "--host" || arg == "--source-host" ||
            arg == "--error" || arg == "--token" || arg == "--start-page" || arg == "--end-page" ||
            arg == "--loop-count" || arg == "--mode") {
            ++i;
            continue;
        }
        if (!arg.starts_with("--")) {
            return arg;
        }
    }
    return {};
}

int optionInt(int argc, char** argv, const std::string& name, int fallback) {
    const auto value = optionValue(argc, argv, name);
    if (value.empty()) {
        return fallback;
    }
    return std::stoi(value);
}

} // namespace

CliOptions parseOptions(int argc, char** argv) {
    CliOptions options;
    options.command = commandName(argc, argv);
    options.configPath = optionValue(argc, argv, "--config");
    options.mode = optionValue(argc, argv, "--mode");
    options.url = optionValue(argc, argv, "--url");
    options.token = optionValue(argc, argv, "--token");
    options.host = optionValue(argc, argv, "--host");
    options.sourceHost = optionValue(argc, argv, "--source-host");
    options.errorMessage = optionValue(argc, argv, "--error");
    options.loopCount = optionInt(argc, argv, "--loop-count", 0);
    options.startPage = optionInt(argc, argv, "--start-page", 4);
    options.endPage = optionInt(argc, argv, "--end-page", 39);
    options.once = hasFlag(argc, argv, "--once");
    options.mock = hasFlag(argc, argv, "--mock");
    return options;
}

std::filesystem::path resolveConfigPath(const std::filesystem::path& requested, const char* argv0) {
    if (!requested.empty()) {
        return asConfigFile(requested);
    }

    const auto exeDir = executableDir(argv0);
    const std::vector<std::filesystem::path> candidates {
        std::filesystem::current_path() / "bin/data/config/pi01.example.yaml",
        exeDir / "data/config/pi01.example.yaml",
        std::filesystem::current_path() / "config/pi01.example.yaml",
        sourceDir() / "config/pi01.example.yaml",
    };

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    return sourceDir() / "config/pi01.example.yaml";
}

tcx::nfc::Result<AppConfig> loadConfig(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) {
        return tcx::nfc::Result<AppConfig>::failure("config not found: " + path.string());
    }

    AppConfig config;
    try {
        const auto values = parseSimpleYaml(path);
        config.deviceId = stringValue(values, "device.device_id", config.deviceId);
        config.readerId = stringValue(values, "device.reader_id", config.readerId);
        config.locationName = stringValue(values, "device.location_name", config.locationName);
        config.siteId = stringValue(values, "device.site_id", config.siteId);
        config.preventDuplicateWriteSec = intValue(values, "app.prevent_duplicate_write_sec", config.preventDuplicateWriteSec);
        config.readerHost = stringValue(values, "rfid.reader_host", config.readerHost);
        config.readerSourceHost = stringValue(values, "rfid.reader_source_host", config.readerSourceHost);
        config.readerPort = intValue(values, "rfid.reader_port", config.readerPort);
        config.ntagStartPage = intValue(values, "ntag.start_page", config.ntagStartPage);
        config.ntagMaxUserBytes = intValue(values, "ntag.max_user_bytes", config.ntagMaxUserBytes);
        config.fixedFallbackUrl = stringValue(values, "url.fixed_fallback_url", config.fixedFallbackUrl);
        config.sqlitePath = stringValue(values, "storage.sqlite_path", config.sqlitePath.string());
        config.eventJsonlPath = stringValue(values, "storage.event_jsonl_path", config.eventJsonlPath.string());
    } catch (const std::exception& ex) {
        return tcx::nfc::Result<AppConfig>::failure("config parse failed: " + std::string(ex.what()));
    }

    if (config.fixedFallbackUrl.empty()) {
        return tcx::nfc::Result<AppConfig>::failure("url.fixed_fallback_url is empty");
    }
    return tcx::nfc::Result<AppConfig>::success(std::move(config));
}

tcx::nfc::TcpEndpoint endpointFor(const AppConfig& config) {
    tcx::nfc::TcpEndpoint endpoint;
    endpoint.host = config.readerHost;
    endpoint.sourceHost = config.readerSourceHost;
    endpoint.port = static_cast<uint16_t>(config.readerPort);
    endpoint.timeoutMs = 2000;
    return endpoint;
}

void printMainUsage() {
    std::cerr << "usage: mayaRFID [--config <file-or-dir>] [--mode headless|mock] [--once] [--loop-count N] [--mock]\n";
    std::cerr << "       GUI is the separate mayaRFID_gui target.\n";
}

void printCliUsage() {
    std::cerr << "usage: mayaRFID_cli [--config <file-or-dir>] <config-check|storage-check|sync-peek|sync-ack|sync-fail|token-add|token-status|build-ndef|reader-ping|read-uid|write-url|read-ndef|mock-once> [--url https://...] [--token TOKEN] [--start-page N] [--end-page N]\n";
}

} // namespace maya_rfid
