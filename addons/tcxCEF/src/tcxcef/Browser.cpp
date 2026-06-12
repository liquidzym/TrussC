#include "tcxcef/Browser.h"

#include "tcxcef/RuntimePaths.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if TCXCEF_HAS_CEF
#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <limits.h>
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <limits.h>
#include <unistd.h>
#endif
#include "include/cef_app.h"
#include "include/cef_browser.h"
#include "include/cef_browser_process_handler.h"
#include "include/cef_client.h"
#include "include/cef_command_line.h"
#include "include/cef_display_handler.h"
#include "include/cef_load_handler.h"
#include "include/cef_permission_handler.h"
#if defined(__APPLE__)
#include "include/views/cef_browser_view.h"
#include "include/views/cef_fill_layout.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_library_loader.h"
#endif
#include "include/wrapper/cef_helpers.h"
#endif

namespace tcxCEF {

class Browser::Impl {
public:
    bool setup(const BrowserSettings& settings);
    void update();
    void shutdown();
    bool isReady() const;
    bool isAvailable() const;
    std::string lastError() const;

#if TCXCEF_HAS_CEF
    bool createBrowserOnUiThread();
#if defined(__APPLE__)
    static bool queueBrowserCreationUntilContextReady(Impl* impl);
    static void removePendingBrowserCreation(Impl* impl);
    static void drainPendingBrowserCreations();
    static std::mutex& pendingBrowserCreationMutex();
    static std::vector<Impl*>& pendingBrowserCreations();
#endif
#endif

private:
    BrowserSettings settings_;
    std::string lastError_;
    bool ready_ = false;
    mutable std::mutex stateMutex_;

    void setReady(bool ready);
    void setLastError(std::string error);
    void clearLastError();

#if TCXCEF_HAS_CEF
    class App;
    class Client;
#if defined(__APPLE__)
    class MacBrowserViewDelegate;
    class MacWindowDelegate;
#endif
    static bool ensureCefInitialized(std::string& error);
    CefRefPtr<Client> client_;
    CefRefPtr<CefBrowser> browser_;
#if defined(__APPLE__)
    CefRefPtr<CefBrowserView> browserView_;
    CefRefPtr<CefWindow> window_;
#endif
    friend class Client;
#if defined(__APPLE__)
    friend class MacBrowserViewDelegate;
    friend class MacWindowDelegate;
#endif
#endif
};

#if TCXCEF_HAS_CEF
#if defined(__APPLE__)
void installMacApplicationHooks();
#endif

namespace {

std::mutex gCefMutex;
bool gCefInitialized = false;
std::atomic<bool> gCefSubprocessDispatchChecked{false};

#if defined(__APPLE__)
std::unique_ptr<CefScopedLibraryLoader> gLibraryLoader;
std::atomic<int64_t> gNextMessagePumpWorkMs{-1};
std::atomic<bool> gCefContextInitialized{false};

int64_t steadyClockMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void scheduleMessagePumpWork(int64_t delayMs) {
    const int64_t next = steadyClockMillis() + (delayMs > 0 ? delayMs : 0);
    gNextMessagePumpWorkMs.store(next, std::memory_order_release);
}

bool ensureCefLibraryLoaded(std::string& error) {
    if (gLibraryLoader) {
        return true;
    }

    auto loader = std::make_unique<CefScopedLibraryLoader>();
    if (!loader->LoadInMain()) {
        error = "Failed to load CEF framework in main process";
        return false;
    }
    gLibraryLoader = std::move(loader);
    return true;
}

#endif

std::string currentExecutablePath() {
#if defined(_WIN32)
    std::array<wchar_t, 4096> buffer{};
    const DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (size > 0 && size < buffer.size()) {
        return std::filesystem::path(buffer.data()).string();
    }
    return {};
#elif defined(__APPLE__)
    uint32_t size = PATH_MAX;
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        buffer.assign(size, '\0');
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
            return {};
        }
    }
    return std::filesystem::weakly_canonical(buffer.data()).string();
#elif defined(__linux__)
    std::array<char, PATH_MAX> buffer{};
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0) {
        return {};
    }
    buffer[static_cast<size_t>(length)] = '\0';
    return std::filesystem::weakly_canonical(buffer.data()).string();
#else
    return {};
#endif
}

#if !defined(_WIN32)
struct MainArgsStorage {
    MainArgsStorage() {
        executable = currentExecutablePath();
        if (executable.empty()) {
            executable = "tcxCEF";
        }
        argv[0] = executable.data();
        argv[1] = nullptr;
    }

    int argc = 1;
    std::string executable;
    std::array<char*, 2> argv{};
};

MainArgsStorage& mainArgsStorage() {
    static MainArgsStorage storage;
    return storage;
}
#endif

std::filesystem::path currentExecutableDir() {
    const auto executable = std::filesystem::path(currentExecutablePath());
    if (!executable.empty()) {
        return executable.parent_path();
    }
    return std::filesystem::current_path();
}

std::filesystem::path defaultDesktopCacheRootPath() {
    return currentExecutableDir() / "data" / "workflows" / "cache" / "cef";
}

std::filesystem::path defaultDesktopLogPath() {
    return currentExecutableDir() / "data" / "workflows" / "logs" / "cef-debug.log";
}

#if defined(__APPLE__)
std::filesystem::path currentBundleContentsDir() {
    const auto executable = std::filesystem::path(currentExecutablePath());
    if (executable.empty()) {
        return {};
    }
    return executable.parent_path().parent_path();
}

std::filesystem::path defaultSubprocessPath(std::string& error) {
    const auto executable = std::filesystem::path(currentExecutablePath());
    if (executable.empty()) {
        error = "Unable to resolve current executable path for CEF subprocess lookup";
        return {};
    }

    const auto contentsDir = executable.parent_path().parent_path();
    const auto appName = executable.filename().string();
    const auto helperName = appName + " Helper";
    const auto helperPath =
        contentsDir / "Frameworks" / (helperName + ".app") / "Contents" / "MacOS" / helperName;
    if (!std::filesystem::is_regular_file(helperPath)) {
        error = "CEF subprocess helper is missing: " + helperPath.string();
        return {};
    }
    return helperPath;
}

std::filesystem::path defaultCacheRootPath() {
    const auto executable = std::filesystem::path(currentExecutablePath());
    const std::string appName = executable.empty() ? "tcxCEF" : executable.filename().string();
    if (const char* home = std::getenv("HOME"); home && std::strlen(home) > 0) {
        return std::filesystem::path(home) / "Library" / "Application Support" / "TrussC" / appName /
               "CEF";
    }
    return currentBundleContentsDir() / "Resources" / "CEFCache";
}
#endif

} // namespace

void appendDefaultCefSwitches(const CefString& processType, CefRefPtr<CefCommandLine> commandLine) {
    commandLine->AppendSwitch("disable-background-networking");
    commandLine->AppendSwitch("disable-client-side-phishing-detection");
    commandLine->AppendSwitch("disable-component-update");
    commandLine->AppendSwitch("disable-default-apps");
    commandLine->AppendSwitch("disable-domain-reliability");
    commandLine->AppendSwitch("disable-gpu");
    commandLine->AppendSwitch("disable-gpu-compositing");
    commandLine->AppendSwitch("disable-gpu-shader-disk-cache");
    commandLine->AppendSwitch("disable-notifications");
    commandLine->AppendSwitch("disable-sync");
    commandLine->AppendSwitch("disable-web-resources");
    commandLine->AppendSwitch("enable-media-stream");
    commandLine->AppendSwitch("metrics-recording-only");
    commandLine->AppendSwitch("no-first-run");
    commandLine->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");
    if (processType.empty()) {
#if defined(__APPLE__)
        commandLine->AppendSwitch("use-alloy-style");
        commandLine->AppendSwitch("use-mock-keychain");
#endif
    }
}

class Browser::Impl::App : public CefApp, public CefBrowserProcessHandler {
public:
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
        return this;
    }

    void OnBeforeCommandLineProcessing(const CefString& processType,
                                       CefRefPtr<CefCommandLine> commandLine) override {
        appendDefaultCefSwitches(processType, commandLine);
    }

#if defined(__APPLE__)
    void OnContextInitialized() override {
        CEF_REQUIRE_UI_THREAD();
        gCefContextInitialized.store(true, std::memory_order_release);
        Browser::Impl::drainPendingBrowserCreations();
    }

    void OnScheduleMessagePumpWork(int64_t delayMs) override {
        scheduleMessagePumpWork(delayMs);
    }
#endif

private:
    IMPLEMENT_REFCOUNTING(App);
};

class EarlySubprocessApp : public CefApp {
public:
    void OnBeforeCommandLineProcessing(const CefString& processType,
                                       CefRefPtr<CefCommandLine> commandLine) override {
        appendDefaultCefSwitches(processType, commandLine);
    }

private:
    IMPLEMENT_REFCOUNTING(EarlySubprocessApp);
};

int executeSubprocess() {
#if defined(__APPLE__)
    return -1;
#else
#if defined(_WIN32)
    CefMainArgs mainArgs(GetModuleHandle(nullptr));
#else
    auto& argvStorage = mainArgsStorage();
    CefMainArgs mainArgs(argvStorage.argc, argvStorage.argv.data());
#endif
    CefRefPtr<CefApp> app = new EarlySubprocessApp();
    const int exitCode = CefExecuteProcess(mainArgs, app, nullptr);
    gCefSubprocessDispatchChecked.store(true, std::memory_order_release);
    return exitCode;
#endif
}

class Browser::Impl::Client : public CefClient,
                              public CefLifeSpanHandler,
                              public CefDisplayHandler,
                              public CefLoadHandler,
                              public CefPermissionHandler {
public:
    explicit Client(Browser::Impl* owner) : owner_(owner) {}

    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
        return this;
    }

    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override {
        return this;
    }

    CefRefPtr<CefLoadHandler> GetLoadHandler() override {
        return this;
    }

    CefRefPtr<CefPermissionHandler> GetPermissionHandler() override {
        return this;
    }

    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
        CEF_REQUIRE_UI_THREAD();
        owner_->browser_ = browser;
        owner_->setReady(true);
        if (owner_->settings_.openDevTools) {
            CefWindowInfo windowInfo;
#if defined(_WIN32)
            windowInfo.SetAsPopup(nullptr, "tcxCEF DevTools");
#elif defined(__APPLE__)
            (void)windowInfo;
#else
            windowInfo.SetAsPopup(0, "tcxCEF DevTools");
#endif
            CefBrowserSettings browserSettings;
            browser->GetHost()->ShowDevTools(windowInfo, this, browserSettings, CefPoint());
        }
    }

    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
        CEF_REQUIRE_UI_THREAD();
        if (owner_->browser_ && owner_->browser_->IsSame(browser)) {
            owner_->browser_ = nullptr;
            owner_->setReady(false);
        }
    }

    void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override {
        CEF_REQUIRE_UI_THREAD();
#if defined(__APPLE__)
        if (auto browserView = CefBrowserView::GetForBrowser(browser)) {
            if (auto window = browserView->GetWindow()) {
                window->SetTitle(title);
            }
        }
#else
        (void)browser;
        (void)title;
#endif
    }

    void OnLoadStart(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, TransitionType) override {
        CEF_REQUIRE_UI_THREAD();
        (void)frame;
    }

    void OnLoadEnd(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame> frame, int httpStatusCode) override {
        CEF_REQUIRE_UI_THREAD();
        if (!frame || !frame->IsMain()) {
            return;
        }
        if (httpStatusCode >= 400) {
            owner_->setLastError("CEF HTTP load failed: " + std::to_string(httpStatusCode) +
                                 " " + frame->GetURL().ToString());
        } else {
            owner_->clearLastError();
        }
    }

    void OnLoadError(CefRefPtr<CefBrowser>,
                     CefRefPtr<CefFrame> frame,
                     ErrorCode errorCode,
                     const CefString& errorText,
                     const CefString& failedUrl) override {
        CEF_REQUIRE_UI_THREAD();
        if (errorCode == ERR_ABORTED || (frame && !frame->IsMain())) {
            return;
        }

        owner_->setLastError("CEF load error: " + errorText.ToString() + " (" +
                             std::to_string(static_cast<int>(errorCode)) + ") " +
                             failedUrl.ToString());
    }

    bool OnConsoleMessage(CefRefPtr<CefBrowser>,
                          cef_log_severity_t level,
                          const CefString& message,
                          const CefString& source,
                          int line) override {
        CEF_REQUIRE_UI_THREAD();
        if (level >= LOGSEVERITY_ERROR) {
            owner_->setLastError("CEF console: " + message.ToString() + " (" +
                                 source.ToString() + ":" + std::to_string(line) + ")");
        }
        return false;
    }

    bool OnRequestMediaAccessPermission(CefRefPtr<CefBrowser>,
                                        CefRefPtr<CefFrame>,
                                        const CefString& requestingOrigin,
                                        uint32_t requestedPermissions,
                                        CefRefPtr<CefMediaAccessCallback> callback) override {
        const std::string origin = requestingOrigin.ToString();
        const bool isLocalOrigin =
            origin.rfind("http://127.0.0.1:", 0) == 0 || origin.rfind("http://localhost:", 0) == 0;
        const uint32_t capturePermissions =
            CEF_MEDIA_PERMISSION_DEVICE_AUDIO_CAPTURE | CEF_MEDIA_PERMISSION_DEVICE_VIDEO_CAPTURE;
        if (isLocalOrigin && (requestedPermissions & capturePermissions) != 0) {
            callback->Continue(requestedPermissions & capturePermissions);
        } else {
            callback->Cancel();
        }
        return true;
    }

private:
    Browser::Impl* owner_ = nullptr;
    IMPLEMENT_REFCOUNTING(Client);
};

#if defined(__APPLE__)
class Browser::Impl::MacWindowDelegate : public CefWindowDelegate {
public:
    MacWindowDelegate(Browser::Impl* owner,
                      CefRefPtr<CefBrowserView> browserView,
                      int width,
                      int height,
                      cef_show_state_t showState)
        : owner_(owner),
          browserView_(browserView),
          width_(width > 0 ? width : 960),
          height_(height > 0 ? height : 720),
          showState_(showState) {}

    void OnWindowCreated(CefRefPtr<CefWindow> window) override {
        CEF_REQUIRE_UI_THREAD();
        owner_->window_ = window;
        owner_->browserView_ = browserView_;
        window->SetTitle("tcxCEF");
        window->SetToFillLayout();
        window->AddChildView(browserView_);
        window->CenterWindow(CefSize(width_, height_));
        window->Layout();
        if (showState_ != CEF_SHOW_STATE_HIDDEN) {
            window->Show();
        }
    }

    void OnWindowDestroyed(CefRefPtr<CefWindow>) override {
        CEF_REQUIRE_UI_THREAD();
        if (owner_) {
            owner_->window_ = nullptr;
            owner_->browserView_ = nullptr;
        }
        browserView_ = nullptr;
    }

    bool CanClose(CefRefPtr<CefWindow>) override {
        CEF_REQUIRE_UI_THREAD();
        if (browserView_) {
            if (auto browser = browserView_->GetBrowser()) {
                return browser->GetHost()->TryCloseBrowser();
            }
        }
        return true;
    }

    CefSize GetPreferredSize(CefRefPtr<CefView>) override {
        return CefSize(width_, height_);
    }

    cef_show_state_t GetInitialShowState(CefRefPtr<CefWindow>) override {
        return showState_;
    }

    cef_runtime_style_t GetWindowRuntimeStyle() override {
        return CEF_RUNTIME_STYLE_ALLOY;
    }

private:
    Browser::Impl* owner_ = nullptr;
    CefRefPtr<CefBrowserView> browserView_;
    int width_ = 960;
    int height_ = 720;
    cef_show_state_t showState_ = CEF_SHOW_STATE_NORMAL;

    IMPLEMENT_REFCOUNTING(MacWindowDelegate);
};

class Browser::Impl::MacBrowserViewDelegate : public CefBrowserViewDelegate {
public:
    explicit MacBrowserViewDelegate(Browser::Impl* owner) : owner_(owner) {}

    bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView>,
                                   CefRefPtr<CefBrowserView> popupBrowserView,
                                   bool isDevTools) override {
        CEF_REQUIRE_UI_THREAD();
        const int width = isDevTools ? 960 : owner_->settings_.width;
        const int height = isDevTools ? 700 : owner_->settings_.height;
        CefWindow::CreateTopLevelWindow(new MacWindowDelegate(
            owner_,
            popupBrowserView,
            width,
            height,
            CEF_SHOW_STATE_NORMAL));
        return true;
    }

    cef_runtime_style_t GetBrowserRuntimeStyle() override {
        return CEF_RUNTIME_STYLE_ALLOY;
    }

private:
    Browser::Impl* owner_ = nullptr;

    IMPLEMENT_REFCOUNTING(MacBrowserViewDelegate);
};

std::mutex& Browser::Impl::pendingBrowserCreationMutex() {
    static std::mutex pendingMutex;
    return pendingMutex;
}

std::vector<Browser::Impl*>& Browser::Impl::pendingBrowserCreations() {
    static std::vector<Impl*> pending;
    return pending;
}

bool Browser::Impl::queueBrowserCreationUntilContextReady(Impl* impl) {
    if (gCefContextInitialized.load(std::memory_order_acquire)) {
        return false;
    }

    auto& pending = pendingBrowserCreations();
    std::lock_guard<std::mutex> lock(pendingBrowserCreationMutex());
    if (gCefContextInitialized.load(std::memory_order_acquire)) {
        return false;
    }
    if (std::find(pending.begin(), pending.end(), impl) == pending.end()) {
        pending.push_back(impl);
    }
    scheduleMessagePumpWork(0);
    return true;
}

void Browser::Impl::removePendingBrowserCreation(Impl* impl) {
    auto& pending = pendingBrowserCreations();
    std::lock_guard<std::mutex> lock(pendingBrowserCreationMutex());
    pending.erase(std::remove(pending.begin(), pending.end(), impl), pending.end());
}

void Browser::Impl::drainPendingBrowserCreations() {
    std::vector<Impl*> toCreate;
    {
        auto& pending = pendingBrowserCreations();
        std::lock_guard<std::mutex> lock(pendingBrowserCreationMutex());
        toCreate.swap(pending);
    }

    for (auto* impl : toCreate) {
        if (impl) {
            impl->createBrowserOnUiThread();
        }
    }
}
#endif

bool Browser::Impl::ensureCefInitialized(std::string& error) {
    std::lock_guard<std::mutex> lock(gCefMutex);
    if (gCefInitialized) {
        return true;
    }

#if defined(__APPLE__)
    installMacApplicationHooks();
    if (!ensureCefLibraryLoaded(error)) {
        return false;
    }
#endif

#if defined(_WIN32)
    CefMainArgs mainArgs(GetModuleHandle(nullptr));
#else
    auto& argvStorage = mainArgsStorage();
    CefMainArgs mainArgs(argvStorage.argc, argvStorage.argv.data());
#endif

    CefRefPtr<Browser::Impl::App> app = new Browser::Impl::App();
#if !defined(__APPLE__)
    if (!gCefSubprocessDispatchChecked.load(std::memory_order_acquire)) {
        const int exitCode = CefExecuteProcess(mainArgs, app, nullptr);
        gCefSubprocessDispatchChecked.store(true, std::memory_order_release);
        if (exitCode >= 0) {
            error = "CEF subprocess returned before browser initialization";
            return false;
        }
    }
#endif

    CefSettings settings;
    settings.no_sandbox = true;
    settings.windowless_rendering_enabled = false;
#if defined(__APPLE__)
    settings.external_message_pump = true;
    const auto subprocessPath = defaultSubprocessPath(error);
    if (subprocessPath.empty()) {
        return false;
    }
    CefString(&settings.browser_subprocess_path).FromString(subprocessPath.string());
    const auto frameworkPath = currentBundleContentsDir() / "Frameworks" / "Chromium Embedded Framework.framework";
    if (std::filesystem::is_directory(frameworkPath)) {
        CefString(&settings.framework_dir_path).FromString(frameworkPath.string());
    }
    const auto cacheRootPath = defaultCacheRootPath();
    std::error_code cacheError;
    std::filesystem::create_directories(cacheRootPath, cacheError);
    CefString(&settings.root_cache_path).FromString(cacheRootPath.string());
    CefString(&settings.cache_path).FromString((cacheRootPath / "Default").string());
#else
    settings.multi_threaded_message_loop = true;
    const auto executablePath = std::filesystem::path(currentExecutablePath());
    const auto executableDir = currentExecutableDir();
    if (!executablePath.empty()) {
        CefString(&settings.browser_subprocess_path).FromString(executablePath.string());
    }
    CefString(&settings.resources_dir_path).FromString(executableDir.string());
    CefString(&settings.locales_dir_path).FromString((executableDir / "locales").string());
    CefString(&settings.locale).FromString("zh-CN");
    CefString(&settings.accept_language_list).FromString("zh-CN,zh,en-US,en");
    const auto cacheRootPath = defaultDesktopCacheRootPath();
    std::error_code cacheError;
    std::filesystem::create_directories(cacheRootPath, cacheError);
    CefString(&settings.root_cache_path).FromString(cacheRootPath.string());
    CefString(&settings.cache_path).FromString((cacheRootPath / "Default").string());
    const auto logPath = defaultDesktopLogPath();
    std::error_code logError;
    std::filesystem::create_directories(logPath.parent_path(), logError);
    CefString(&settings.log_file).FromString(logPath.string());
    settings.log_severity = LOGSEVERITY_INFO;
#endif

    if (!CefInitialize(mainArgs, settings, app, nullptr)) {
        error = "CefInitialize failed";
        return false;
    }
#if defined(__APPLE__)
    scheduleMessagePumpWork(0);
#endif
    gCefInitialized = true;
    return true;
}
#endif

bool Browser::Impl::setup(const BrowserSettings& settings) {
    shutdown();
    settings_ = settings;
    clearLastError();

    if (settings_.url.empty()) {
        setLastError("Browser URL is empty");
        return false;
    }

#if !TCXCEF_HAS_CEF
    setLastError("CEF runtime is not setup. Run python addons/tcxCEF/tools/setup_cef.py --config Release");
    return false;
#else
    std::string error;
    if (!ensureCefInitialized(error)) {
        setLastError(error);
        return false;
    }

    client_ = new Client(this);

#if defined(__APPLE__)
    if (queueBrowserCreationUntilContextReady(this)) {
        return true;
    }
#endif

    return createBrowserOnUiThread();
#endif
}

#if TCXCEF_HAS_CEF
bool Browser::Impl::createBrowserOnUiThread() {
    if (!client_) {
        return false;
    }
    if (browser_
#if defined(__APPLE__)
        || browserView_ || window_
#endif
    ) {
        return true;
    }

    CefBrowserSettings browserSettings;
#if defined(__APPLE__)
    if (settings_.showWindow) {
        browserSettings.background_color = CefColorSetARGB(255, 16, 19, 23);
        browserView_ = CefBrowserView::CreateBrowserView(
            client_,
            settings_.url,
            browserSettings,
            nullptr,
            nullptr,
            new MacBrowserViewDelegate(this));
        if (!browserView_) {
            setLastError("CefBrowserView::CreateBrowserView failed");
            client_ = nullptr;
            return false;
        }

        window_ = CefWindow::CreateTopLevelWindow(new MacWindowDelegate(
            this,
            browserView_,
            settings_.width,
            settings_.height,
            CEF_SHOW_STATE_NORMAL));
        if (!window_) {
            setLastError("CefWindow::CreateTopLevelWindow failed");
            browserView_ = nullptr;
            client_ = nullptr;
            return false;
        }
        return true;
    }
#endif

    CefWindowInfo windowInfo;
    if (settings_.showWindow) {
#if defined(_WIN32)
        windowInfo.SetAsPopup(nullptr, "tcxCEF");
#elif defined(__APPLE__)
        (void)windowInfo;
#else
        windowInfo.SetAsPopup(0, "tcxCEF");
#endif
    } else {
#if defined(_WIN32)
        windowInfo.SetAsWindowless(nullptr);
#elif defined(__APPLE__)
        windowInfo.SetAsWindowless(nullptr);
#else
        windowInfo.SetAsWindowless(0);
#endif
    }

#if defined(__APPLE__)
    windowInfo.runtime_style = CEF_RUNTIME_STYLE_ALLOY;
#endif
    const bool created = CefBrowserHost::CreateBrowser(
        windowInfo,
        client_,
        settings_.url,
        browserSettings,
        nullptr,
        nullptr);
    if (!created) {
        setLastError("CefBrowserHost::CreateBrowser failed");
        client_ = nullptr;
        return false;
    }
    return true;
}
#endif

#if !TCXCEF_HAS_CEF
int executeSubprocess() {
    return -1;
}
#endif

void Browser::Impl::update() {
#if TCXCEF_HAS_CEF && defined(__APPLE__)
    if (gCefInitialized) {
        CefDoMessageLoopWork();
        const int64_t next = gNextMessagePumpWorkMs.load(std::memory_order_acquire);
        if (next >= 0 && steadyClockMillis() >= next) {
            gNextMessagePumpWorkMs.store(-1, std::memory_order_release);
        }
    }
#endif
}

void Browser::Impl::shutdown() {
#if TCXCEF_HAS_CEF
#if defined(__APPLE__)
    removePendingBrowserCreation(this);
#endif
    if (browser_) {
        browser_->GetHost()->CloseBrowser(true);
        browser_ = nullptr;
    }
#if defined(__APPLE__)
    if (window_ && !window_->IsClosed()) {
        window_->Close();
    }
    window_ = nullptr;
    browserView_ = nullptr;
#endif
    client_ = nullptr;
#endif
    setReady(false);
}

bool Browser::Impl::isReady() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return ready_;
}

bool Browser::Impl::isAvailable() const {
    return isCefAvailable();
}

std::string Browser::Impl::lastError() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return lastError_;
}

void Browser::Impl::setReady(bool ready) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    ready_ = ready;
}

void Browser::Impl::setLastError(std::string error) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    lastError_ = std::move(error);
}

void Browser::Impl::clearLastError() {
    std::lock_guard<std::mutex> lock(stateMutex_);
    lastError_.clear();
}

Browser::Browser() : impl_(std::make_unique<Impl>()) {}

Browser::~Browser() {
    shutdown();
}

bool Browser::setup(const BrowserSettings& settings) {
    return impl_->setup(settings);
}

void Browser::update() {
    impl_->update();
}

void Browser::shutdown() {
    impl_->shutdown();
}

bool Browser::isReady() const {
    return impl_->isReady();
}

bool Browser::isAvailable() const {
    return impl_->isAvailable();
}

std::string Browser::lastError() const {
    return impl_->lastError();
}

} // namespace tcxCEF
