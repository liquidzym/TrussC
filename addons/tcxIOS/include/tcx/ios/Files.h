#pragma once

#include "Types.h"
#include "Operations.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace tcx::ios {

enum class AppDirectory {
    Documents,
    Caches,
    Temporary,
    ApplicationSupport
};

struct PickedFile {
    std::filesystem::path localPath;
    std::string contentType;
    bool copiedIntoSandbox = true;
    std::vector<std::uint8_t> securityScopedBookmark;

    // Compatibility aliases for the initial v0 API. Prefer localPath/contentType.
    std::filesystem::path path;
    std::string uti;
    bool securityScoped = false;
};

struct ImportFileRequest {
    std::vector<std::string> contentTypes;
    bool allowMultiple = false;
    bool copyIntoApp = true;
};

struct ExportFileRequest {
    std::filesystem::path path;
    std::string suggestedName;
};

class Files {
public:
    std::filesystem::path directoryPath(AppDirectory directory) const;
    std::filesystem::path documentsDirectory() const;
    std::filesystem::path cachesDirectory() const;
    std::filesystem::path temporaryDirectory() const;
    std::filesystem::path applicationSupportDirectory() const;

    void importFiles(const ImportFileRequest& request, Completion<std::vector<PickedFile>> done);
    OperationHandle importFilesCancellable(const ImportFileRequest& request,
                                           Completion<std::vector<PickedFile>> done);
    void exportFile(const ExportFileRequest& request, Completion<void> done);
    OperationHandle exportFileCancellable(const ExportFileRequest& request, Completion<void> done);
    void stopAccessing(const PickedFile& file);
};

Files& files();

std::string toString(AppDirectory directory);

} // namespace tcx::ios
