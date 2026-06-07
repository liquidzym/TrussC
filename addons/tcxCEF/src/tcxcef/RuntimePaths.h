#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace tcxCEF {

struct CefRuntimePaths {
    bool available = false;
    std::string platform;
    std::string version;
    std::filesystem::path addonRoot;
    std::filesystem::path cefRoot;
    std::filesystem::path includeDir;
    std::filesystem::path releaseDir;
    std::filesystem::path resourceDir;
    std::filesystem::path libcefLibrary;
    std::filesystem::path cefFrameworkPath;
    std::filesystem::path wrapperLibrary;
    std::vector<std::filesystem::path> runtimeFiles;
    std::vector<std::filesystem::path> resourceFiles;
};

std::filesystem::path addonRoot();
bool isCefAvailable();
CefRuntimePaths runtimePaths();

} // namespace tcxCEF
