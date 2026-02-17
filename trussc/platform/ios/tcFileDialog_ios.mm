// =============================================================================
// iOS file dialog stub implementation
// Full implementation (UIAlertController, UIDocumentPicker) planned for Phase 2
// =============================================================================

#include "tc/utils/tcFileDialog.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IOS

#include "tc/utils/tcLog.h"

namespace trussc {

// -----------------------------------------------------------------------------
// Alert dialog (stub)
// -----------------------------------------------------------------------------
void alertDialog(const std::string& title, const std::string& message) {
    logWarning() << "[FileDialog] alertDialog not yet supported on iOS: " << title;
}

void alertDialogAsync(const std::string& title,
                      const std::string& message,
                      std::function<void()> callback) {
    logWarning() << "[FileDialog] alertDialogAsync not yet supported on iOS: " << title;
    if (callback) callback();
}

// -----------------------------------------------------------------------------
// Confirm dialog (stub)
// -----------------------------------------------------------------------------
bool confirmDialog(const std::string& title, const std::string& message) {
    logWarning() << "[FileDialog] confirmDialog not yet supported on iOS: " << title;
    return false;
}

void confirmDialogAsync(const std::string& title,
                        const std::string& message,
                        std::function<void(bool)> callback) {
    logWarning() << "[FileDialog] confirmDialogAsync not yet supported on iOS: " << title;
    if (callback) callback(false);
}

// -----------------------------------------------------------------------------
// Load dialog (stub)
// -----------------------------------------------------------------------------
FileDialogResult loadDialog(const std::string& title,
                            const std::string& message,
                            const std::string& defaultPath,
                            bool folderSelection) {
    logWarning() << "[FileDialog] loadDialog not yet supported on iOS";
    return FileDialogResult{};
}

void loadDialogAsync(const std::string& title,
                     const std::string& message,
                     const std::string& defaultPath,
                     bool folderSelection,
                     std::function<void(const FileDialogResult&)> callback) {
    logWarning() << "[FileDialog] loadDialogAsync not yet supported on iOS";
    if (callback) callback(FileDialogResult{});
}

// -----------------------------------------------------------------------------
// Save dialog (stub)
// -----------------------------------------------------------------------------
FileDialogResult saveDialog(const std::string& title,
                            const std::string& message,
                            const std::string& defaultPath,
                            const std::string& defaultName) {
    logWarning() << "[FileDialog] saveDialog not yet supported on iOS";
    return FileDialogResult{};
}

void saveDialogAsync(const std::string& title,
                     const std::string& message,
                     const std::string& defaultPath,
                     const std::string& defaultName,
                     std::function<void(const FileDialogResult&)> callback) {
    logWarning() << "[FileDialog] saveDialogAsync not yet supported on iOS";
    if (callback) callback(FileDialogResult{});
}

} // namespace trussc

#endif // TARGET_OS_IOS
#endif // __APPLE__
