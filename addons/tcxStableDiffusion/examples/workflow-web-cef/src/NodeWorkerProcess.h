#pragma once

#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

struct NodeWorkerSettings {
    std::filesystem::path nodeExecutable;
    std::filesystem::path workerScript;
    std::filesystem::path cwd;
};

class NodeWorkerProcess {
public:
    NodeWorkerProcess() = default;
    ~NodeWorkerProcess();

    NodeWorkerProcess(const NodeWorkerProcess&) = delete;
    NodeWorkerProcess& operator=(const NodeWorkerProcess&) = delete;

    bool start(const NodeWorkerSettings& settings);
    void stop();
    bool send(const std::string& jsonLine);
    void drainMessages(const std::function<void(const std::string&)>& emit);
    bool isRunning() const;
    std::string lastError() const;

private:
    void readerLoop();
    void pushMessage(std::string message);
    void setError(std::string error);

    NodeWorkerSettings settings_;
    mutable std::mutex mutex_;
    std::vector<std::string> messages_;
    std::string lastError_;
    std::thread readerThread_;
    bool running_ = false;

#ifdef _WIN32
    PROCESS_INFORMATION processInfo_{};
    HANDLE childStdinWrite_ = nullptr;
    HANDLE childStdoutRead_ = nullptr;
#endif
};
