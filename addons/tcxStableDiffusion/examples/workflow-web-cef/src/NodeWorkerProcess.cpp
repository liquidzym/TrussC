#include "NodeWorkerProcess.h"

#include <filesystem>
#include <sstream>

namespace {

std::wstring quotePath(const std::filesystem::path& path) {
    std::wstring text = path.wstring();
    std::wstring escaped;
    escaped.reserve(text.size() + 2);
    escaped.push_back(L'"');
    for (wchar_t ch : text) {
        if (ch == L'"') {
            escaped.push_back(L'\\');
        }
        escaped.push_back(ch);
    }
    escaped.push_back(L'"');
    return escaped;
}

std::string pathText(const std::filesystem::path& path) {
    return path.string();
}

} // namespace

NodeWorkerProcess::~NodeWorkerProcess() {
    stop();
}

bool NodeWorkerProcess::start(const NodeWorkerSettings& settings) {
    stop();
    settings_ = settings;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        lastError_.clear();
        messages_.clear();
    }

    if (!std::filesystem::is_regular_file(settings_.nodeExecutable)) {
        setError("WORKER_UNAVAILABLE: bundled node executable is missing: " + pathText(settings_.nodeExecutable));
        return false;
    }
    if (!std::filesystem::is_regular_file(settings_.workerScript)) {
        setError("WORKER_UNAVAILABLE: worker script is missing: " + pathText(settings_.workerScript));
        return false;
    }

#ifdef _WIN32
    SECURITY_ATTRIBUTES pipeSecurity{};
    pipeSecurity.nLength = sizeof(SECURITY_ATTRIBUTES);
    pipeSecurity.bInheritHandle = TRUE;
    pipeSecurity.lpSecurityDescriptor = nullptr;

    HANDLE childStdoutWrite = nullptr;
    HANDLE childStdinRead = nullptr;
    if (!CreatePipe(&childStdoutRead_, &childStdoutWrite, &pipeSecurity, 0)) {
        setError("WORKER_UNAVAILABLE: CreatePipe failed for worker stdout");
        return false;
    }
    if (!SetHandleInformation(childStdoutRead_, HANDLE_FLAG_INHERIT, 0)) {
        setError("WORKER_UNAVAILABLE: SetHandleInformation failed for worker stdout");
        stop();
        CloseHandle(childStdoutWrite);
        return false;
    }
    if (!CreatePipe(&childStdinRead, &childStdinWrite_, &pipeSecurity, 0)) {
        setError("WORKER_UNAVAILABLE: CreatePipe failed for worker stdin");
        stop();
        CloseHandle(childStdoutWrite);
        return false;
    }
    if (!SetHandleInformation(childStdinWrite_, HANDLE_FLAG_INHERIT, 0)) {
        setError("WORKER_UNAVAILABLE: SetHandleInformation failed for worker stdin");
        stop();
        CloseHandle(childStdoutWrite);
        CloseHandle(childStdinRead);
        return false;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(STARTUPINFOW);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = childStdoutWrite;
    startup.hStdError = childStdoutWrite;
    startup.hStdInput = childStdinRead;

    PROCESS_INFORMATION processInfo{};
    std::wstring commandLine = quotePath(settings_.nodeExecutable) + L" " + quotePath(settings_.workerScript);
    std::wstring cwd = settings_.cwd.wstring();
    const BOOL created = CreateProcessW(
        nullptr,
        commandLine.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        cwd.empty() ? nullptr : cwd.c_str(),
        &startup,
        &processInfo);
    CloseHandle(childStdoutWrite);
    CloseHandle(childStdinRead);

    if (!created) {
        setError("WORKER_UNAVAILABLE: CreateProcessW failed for bundled Node worker");
        stop();
        return false;
    }

    processInfo_ = processInfo;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = true;
    }
    readerThread_ = std::thread(&NodeWorkerProcess::readerLoop, this);
    return true;
#else
    setError("WORKER_UNAVAILABLE: NodeWorkerProcess is implemented for Windows in this example");
    return false;
#endif
}

void NodeWorkerProcess::stop() {
#ifdef _WIN32
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }
    if (childStdinWrite_) {
        CloseHandle(childStdinWrite_);
        childStdinWrite_ = nullptr;
    }
    if (processInfo_.hProcess) {
        TerminateProcess(processInfo_.hProcess, 0);
    }
    if (readerThread_.joinable()) {
        readerThread_.join();
    }
    if (childStdoutRead_) {
        CloseHandle(childStdoutRead_);
        childStdoutRead_ = nullptr;
    }
    if (processInfo_.hThread) {
        CloseHandle(processInfo_.hThread);
        processInfo_.hThread = nullptr;
    }
    if (processInfo_.hProcess) {
        CloseHandle(processInfo_.hProcess);
        processInfo_.hProcess = nullptr;
    }
#else
    if (readerThread_.joinable()) {
        readerThread_.join();
    }
#endif
}

bool NodeWorkerProcess::send(const std::string& jsonLine) {
#ifdef _WIN32
    if (!isRunning() || !childStdinWrite_) {
        setError("WORKER_UNAVAILABLE: worker is not running");
        return false;
    }
    std::string line = jsonLine;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.pop_back();
    }
    line.push_back('\n');
    DWORD written = 0;
    if (!WriteFile(childStdinWrite_, line.data(), static_cast<DWORD>(line.size()), &written, nullptr) ||
        written != line.size()) {
        setError("WORKER_UNAVAILABLE: WriteFile failed for worker stdin");
        return false;
    }
    return true;
#else
    setError("WORKER_UNAVAILABLE: worker is not running");
    return false;
#endif
}

void NodeWorkerProcess::drainMessages(const std::function<void(const std::string&)>& emit) {
    std::vector<std::string> pending;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending.swap(messages_);
    }
    for (const auto& message : pending) {
        emit(message);
    }
}

bool NodeWorkerProcess::isRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

std::string NodeWorkerProcess::lastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

void NodeWorkerProcess::readerLoop() {
#ifdef _WIN32
    std::string buffer;
    char chunk[4096];
    while (true) {
        DWORD read = 0;
        const BOOL ok = ReadFile(childStdoutRead_, chunk, static_cast<DWORD>(sizeof(chunk)), &read, nullptr);
        if (!ok || read == 0) {
            break;
        }
        buffer.append(chunk, chunk + read);
        size_t newline = std::string::npos;
        while ((newline = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, newline);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            buffer.erase(0, newline + 1);
            if (!line.empty()) {
                pushMessage(std::move(line));
            }
        }
    }
    if (!buffer.empty()) {
        pushMessage(std::move(buffer));
    }
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
#endif
}

void NodeWorkerProcess::pushMessage(std::string message) {
    std::lock_guard<std::mutex> lock(mutex_);
    messages_.push_back(std::move(message));
}

void NodeWorkerProcess::setError(std::string error) {
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_ = std::move(error);
}
