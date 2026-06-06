#pragma once

#include "Types.h"
#include "Operations.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace tcx::ios {

struct BackgroundDownloadRequest {
    std::string url;
    std::filesystem::path destination;
    std::string identifier;
    std::string sessionIdentifier = "org.trussc.tcxios.background-downloads";
    bool allowsCellularAccess = true;
    bool persistAcrossRelaunch = true;
};

struct BackgroundDownloadProgress {
    std::string identifier;
    std::int64_t bytesWritten = 0;
    std::int64_t totalBytesWritten = 0;
    std::int64_t totalBytesExpected = -1;
    double fractionCompleted = 0.0;
};

struct BackgroundDownloadResult {
    std::string identifier;
    std::filesystem::path file;
    std::string sourceURL;
};

using BackgroundDownloadProgressHandler = std::function<void(const BackgroundDownloadProgress&)>;

class BackgroundDownloads {
public:
    void download(const BackgroundDownloadRequest& request,
                  Completion<BackgroundDownloadResult> done,
                  BackgroundDownloadProgressHandler progress = nullptr);
    OperationHandle downloadCancellable(const BackgroundDownloadRequest& request,
                                        Completion<BackgroundDownloadResult> done,
                                        BackgroundDownloadProgressHandler progress = nullptr);
    void cancel(const std::string& identifier);
    std::vector<BackgroundDownloadRequest> pendingRequests() const;
};

BackgroundDownloads& backgroundDownloads();

} // namespace tcx::ios
