#include "TrussC.h"
#include "tcApp.h"

#include <tcxCEF.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

std::filesystem::path currentExecutablePath() {
#ifdef _WIN32
    std::array<wchar_t, 4096> buffer{};
    const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size > 0 && size < buffer.size()) {
        return std::filesystem::path(buffer.data());
    }
#endif
    return {};
}

std::filesystem::path executableDir() {
    const auto executable = currentExecutablePath();
    if (!executable.empty()) {
        return executable.parent_path();
    }
    return std::filesystem::current_path();
}

std::filesystem::path startupLogPath() {
    return executableDir() / "data" / "workflows" / "logs" / "workflow-web-cef-native.log";
}

void appendStartupLog(const std::string& line) {
    const auto path = startupLogPath();
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream output(path, std::ios::app);
    if (output) {
        output << line << '\n';
    }
}

} // namespace

int main() {
    appendStartupLog("进程启动: " + executableDir().string());
    const int subprocessExitCode = tcxCEF::executeSubprocess();
    if (subprocessExitCode >= 0) {
        appendStartupLog("CEF 子进程退出: " + std::to_string(subprocessExitCode));
        return subprocessExitCode;
    }
    appendStartupLog("主进程继续启动 TrussC");

    tc::WindowSettings settings;
    settings.title = "tcxStableDiffusion 工作流工作台";
    settings.width = 1440;
    settings.height = 920;
    settings.highDpi = false;
    return TC_RUN_APP(tcApp, settings);
}
