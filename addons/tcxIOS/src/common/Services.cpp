#include "tcxIOS.h"
#include "TCXIOSPlatform.h"

#include <TrussC.h>

#include <algorithm>
#include <atomic>
#include <map>
#include <mutex>
#include <utility>

namespace tcx::ios {
namespace {

EventQueue gEventQueue;
App gApp;
Scene gScene;
NativeUI gNativeUI;
Permissions gPermissions;
Files gFiles;
Photos gPhotos;
Camera gCamera;
AudioSession gAudioSession;
Haptics gHaptics;
Motion gMotion;
Notifications gNotifications;
Operations gOperations;
Logger gLogger;
Location gLocation;
NetworkStatus gNetworkStatus;
BackgroundTasks gBackgroundTasks;
BackgroundDownloads gBackgroundDownloads;
Keychain gKeychain;
LocalAuthentication gLocalAuthentication;
Web gWeb;
ExternalDisplay gExternalDisplay;
BluetoothLE gBluetoothLE;
Multipeer gMultipeer;
GameController gGameController;
PencilCanvas gPencilCanvas;
StoreKit gStoreKit;
ContactsUI gContactsUI;
ARKitBridge gARKitBridge;
VisionBridge gVisionBridge;
CoreMLBridge gCoreMLBridge;
std::mutex gOperationMutex;
std::map<std::string, std::shared_ptr<OperationState>> gOperationStates;
std::atomic_uint64_t gOperationCounter{0};
std::mutex gLoggerMutex;
LogHandler gLogHandler;
std::mutex gAudioHandlerMutex;
AudioInterruptionHandler gAudioInterruptionHandler;
AudioRouteChangeHandler gAudioRouteChangeHandler;
std::mutex gExternalDisplayHandlerMutex;
ExternalDisplayChangeHandler gExternalDisplayChangeHandler;

} // namespace

EventQueue& eventQueue() {
    return gEventQueue;
}

void update() {
    gEventQueue.drain();
}

App& app() {
    return gApp;
}

AppState App::state() const {
    return detail::platformAppState();
}

ScreenInfo App::mainScreen() const {
    return detail::platformMainScreen();
}

SafeAreaInsets App::safeAreaInsets() const {
    return detail::platformSafeAreaInsets();
}

Orientation App::orientation() const {
    return detail::platformOrientation();
}

DeviceInfo App::deviceInfo() const {
    return detail::platformDeviceInfo();
}

std::string toString(AppState state) {
    switch (state) {
        case AppState::Inactive: return "inactive";
        case AppState::Active: return "active";
        case AppState::Background: return "background";
    }
    return "unknown";
}

std::string toString(Orientation orientation) {
    switch (orientation) {
        case Orientation::Unknown: return "unknown";
        case Orientation::Portrait: return "portrait";
        case Orientation::PortraitUpsideDown: return "portrait upside down";
        case Orientation::LandscapeLeft: return "landscape left";
        case Orientation::LandscapeRight: return "landscape right";
    }
    return "unknown";
}

Scene& scene() {
    return gScene;
}

void Scene::upsertContext(SceneContext context) {
    std::lock_guard<std::mutex> lock(mutex_);
    bool found = false;
    for (auto& existing : contexts_) {
        if (existing.identifier == context.identifier) {
            existing = context;
            found = true;
            break;
        }
    }
    if (!found) contexts_.push_back(context);
    if (context.active) active_ = std::move(context);
}

void Scene::removeContext(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(mutex_);
    contexts_.erase(std::remove_if(contexts_.begin(), contexts_.end(),
        [&](const SceneContext& context) { return context.identifier == identifier; }),
        contexts_.end());
    if (active_.identifier == identifier) active_ = {};
}

std::vector<SceneContext> Scene::contexts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return contexts_;
}

void Scene::setActiveContext(SceneContext context) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_ = std::move(context);
    bool found = false;
    for (auto& existing : contexts_) {
        if (existing.identifier == active_.identifier) {
            existing = active_;
            found = true;
            break;
        }
    }
    if (!found && !active_.identifier.empty()) contexts_.push_back(active_);
}

void Scene::setActiveIdentifier(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& context : contexts_) {
        context.active = context.identifier == identifier;
        if (context.active) active_ = context;
    }
}

SceneContext Scene::activeContext() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_;
}

std::string Scene::activeIdentifier() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_.identifier;
}

bool Scene::hasActiveScene() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return active_.active;
}

NativeUI& nativeUI() {
    return gNativeUI;
}

void NativeUI::showAlert(const AlertRequest& request, Completion<AlertResult> done) {
    detail::platformShowAlert(request, std::move(done));
}

void NativeUI::share(const ShareRequest& request, Completion<ShareResult> done) {
    detail::platformShare(request, std::move(done));
}

void NativeUI::openSettings() {
    detail::platformOpenSettings();
}

void NativeUI::openURL(const std::string& url, Completion<void> done) {
    detail::platformOpenURL(url, std::move(done));
}

Permissions& permissions() {
    return gPermissions;
}

PermissionState Permissions::status(Permission permission) const {
    return detail::platformPermissionStatus(permission);
}

void Permissions::request(Permission permission, Completion<PermissionState> done) {
    detail::platformRequestPermission(permission, std::move(done));
}

std::string toString(Permission permission) {
    switch (permission) {
        case Permission::Camera: return "camera";
        case Permission::Microphone: return "microphone";
        case Permission::PhotoLibraryRead: return "photo library read";
        case Permission::PhotoLibraryAddOnly: return "photo library add only";
        case Permission::LocationWhenInUse: return "location when in use";
        case Permission::LocationAlways: return "location always";
        case Permission::Notifications: return "notifications";
        case Permission::Bluetooth: return "bluetooth";
        case Permission::Motion: return "motion";
        case Permission::Contacts: return "contacts";
    }
    return "unknown";
}

std::string toString(PermissionState state) {
    switch (state) {
        case PermissionState::Unknown: return "unknown";
        case PermissionState::NotDetermined: return "not determined";
        case PermissionState::Denied: return "denied";
        case PermissionState::Restricted: return "restricted";
        case PermissionState::Authorized: return "authorized";
        case PermissionState::Limited: return "limited";
        case PermissionState::Provisional: return "provisional";
    }
    return "unknown";
}

Operations& operations() {
    return gOperations;
}

OperationHandle::OperationHandle(std::shared_ptr<OperationState> state)
    : state_(std::move(state)) {}

bool OperationHandle::valid() const {
    return static_cast<bool>(state_);
}

const std::string& OperationHandle::identifier() const {
    static const std::string empty;
    return state_ ? state_->identifier : empty;
}

const std::string& OperationHandle::label() const {
    static const std::string empty;
    return state_ ? state_->label : empty;
}

void OperationHandle::cancel() const {
    if (!state_) return;
    const bool wasCancelled = state_->cancelled.exchange(true);
    if (!wasCancelled && state_->cancelHandler) state_->cancelHandler();
}

bool OperationHandle::cancelled() const {
    return state_ && state_->cancelled.load();
}

OperationHandle Operations::create(const std::string& label, std::function<void()> cancelHandler) {
    auto state = std::make_shared<OperationState>();
    state->identifier = "tcxios-operation-" + std::to_string(gOperationCounter.fetch_add(1) + 1);
    state->label = label;
    state->cancelHandler = std::move(cancelHandler);
    {
        std::lock_guard<std::mutex> lock(gOperationMutex);
        gOperationStates[state->identifier] = state;
    }
    return OperationHandle(std::move(state));
}

OperationHandle Operations::get(const std::string& identifier) const {
    std::lock_guard<std::mutex> lock(gOperationMutex);
    auto it = gOperationStates.find(identifier);
    return it == gOperationStates.end() ? OperationHandle() : OperationHandle(it->second);
}

bool Operations::cancel(const std::string& identifier) {
    OperationHandle handle = get(identifier);
    if (!handle.valid()) return false;
    handle.cancel();
    return true;
}

bool Operations::isCancelled(const std::string& identifier) const {
    OperationHandle handle = get(identifier);
    return handle.cancelled();
}

void Operations::remove(const std::string& identifier) {
    std::lock_guard<std::mutex> lock(gOperationMutex);
    gOperationStates.erase(identifier);
}

std::size_t Operations::activeCount() const {
    std::lock_guard<std::mutex> lock(gOperationMutex);
    return gOperationStates.size();
}

Logger& logger() {
    return gLogger;
}

void Logger::setHandler(LogHandler handler) {
    std::lock_guard<std::mutex> lock(gLoggerMutex);
    gLogHandler = std::move(handler);
}

void Logger::clearHandler() {
    std::lock_guard<std::mutex> lock(gLoggerMutex);
    gLogHandler = nullptr;
}

void Logger::log(LogRecord record) {
    LogHandler handler;
    {
        std::lock_guard<std::mutex> lock(gLoggerMutex);
        handler = gLogHandler;
    }
    if (handler) handler(record);
}

void Logger::debug(const std::string& subsystem, const std::string& message) {
    log({LogLevel::Debug, subsystem, message});
}

void Logger::info(const std::string& subsystem, const std::string& message) {
    log({LogLevel::Info, subsystem, message});
}

void Logger::warning(const std::string& subsystem, const std::string& message) {
    log({LogLevel::Warning, subsystem, message});
}

void Logger::error(const std::string& subsystem, const std::string& message, Error nativeError) {
    log({LogLevel::Error, subsystem, message, nativeError});
}

std::string toString(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "debug";
        case LogLevel::Info: return "info";
        case LogLevel::Warning: return "warning";
        case LogLevel::Error: return "error";
    }
    return "unknown";
}

template <typename T>
Completion<T> cancellableCompletion(OperationHandle operation, Completion<T> done) {
    return [operation, done = std::move(done)](Result<T> result) mutable {
        const bool cancelled = operation.cancelled();
        const std::string identifier = operation.identifier();
        if (!cancelled && done) done(std::move(result));
        operations().remove(identifier);
    };
}

Files& files() {
    return gFiles;
}

std::filesystem::path Files::directoryPath(AppDirectory directory) const {
    return detail::platformAppDirectoryPath(directory);
}

std::filesystem::path Files::documentsDirectory() const {
    return directoryPath(AppDirectory::Documents);
}

std::filesystem::path Files::cachesDirectory() const {
    return directoryPath(AppDirectory::Caches);
}

std::filesystem::path Files::temporaryDirectory() const {
    return directoryPath(AppDirectory::Temporary);
}

std::filesystem::path Files::applicationSupportDirectory() const {
    return directoryPath(AppDirectory::ApplicationSupport);
}

void Files::importFiles(const ImportFileRequest& request, Completion<std::vector<PickedFile>> done) {
    detail::platformImportFiles(request, std::move(done));
}

OperationHandle Files::importFilesCancellable(const ImportFileRequest& request,
                                              Completion<std::vector<PickedFile>> done) {
    OperationHandle operation = operations().create("Files.importFiles");
    detail::platformImportFiles(request, cancellableCompletion(operation, std::move(done)));
    return operation;
}

void Files::exportFile(const ExportFileRequest& request, Completion<void> done) {
    detail::platformExportFile(request, std::move(done));
}

OperationHandle Files::exportFileCancellable(const ExportFileRequest& request, Completion<void> done) {
    OperationHandle operation = operations().create("Files.exportFile");
    detail::platformExportFile(request, cancellableCompletion(operation, std::move(done)));
    return operation;
}

void Files::stopAccessing(const PickedFile& file) {
    detail::platformStopAccessingFile(file);
}

std::string toString(AppDirectory directory) {
    switch (directory) {
        case AppDirectory::Documents: return "documents";
        case AppDirectory::Caches: return "caches";
        case AppDirectory::Temporary: return "temporary";
        case AppDirectory::ApplicationSupport: return "application support";
    }
    return "unknown";
}

Photos& photos() {
    return gPhotos;
}

void Photos::pickPhotos(const PhotoPickerRequest& request, Completion<std::vector<PickedPhoto>> done) {
    detail::platformPickPhotos(request, std::move(done));
}

OperationHandle Photos::pickPhotosCancellable(const PhotoPickerRequest& request,
                                              Completion<std::vector<PickedPhoto>> done) {
    OperationHandle operation = operations().create("Photos.pickPhotos");
    detail::platformPickPhotos(request, cancellableCompletion(operation, std::move(done)));
    return operation;
}

void Photos::save(const PhotoSaveRequest& request, Completion<void> done) {
    detail::platformSavePhoto(request, std::move(done));
}

OperationHandle Photos::saveCancellable(const PhotoSaveRequest& request, Completion<void> done) {
    OperationHandle operation = operations().create("Photos.save");
    detail::platformSavePhoto(request, cancellableCompletion(operation, std::move(done)));
    return operation;
}

std::string toString(PhotoMediaType mediaType) {
    switch (mediaType) {
        case PhotoMediaType::Image: return "image";
        case PhotoMediaType::Video: return "video";
        case PhotoMediaType::ImagesAndVideos: return "images and videos";
    }
    return "unknown";
}

Camera& camera() {
    return gCamera;
}

std::vector<CameraDeviceInfo> Camera::availableDevices() const {
    return detail::platformAvailableCameraDevices();
}

void Camera::start(const CameraConfig& config, Completion<void> done) {
    detail::platformStartCamera(config, std::move(done));
}

void Camera::stop() {
    detail::platformStopCamera();
}

bool Camera::isRunning() const {
    return detail::platformCameraIsRunning();
}

bool Camera::latestFrame(CameraFrame& out) const {
    return detail::platformLatestCameraFrame(out);
}

bool Camera::latestFrameView(CameraFrameView& out) const {
    return detail::platformLatestCameraFrameView(out);
}

bool Camera::copyLatestFrameToPixels(trussc::Pixels& dst) const {
    CameraFrame frame;
    if (!latestFrame(frame)) return false;
    if (frame.pixelFormat != CameraPixelFormat::BGRA8) return false;
    if (frame.width <= 0 || frame.height <= 0) return false;
    const std::size_t expected = static_cast<std::size_t>(frame.width) *
                                 static_cast<std::size_t>(frame.height) * 4;
    if (frame.data.size() < expected) return false;

    dst.setFromPixels(frame.data.data(), frame.width, frame.height, 4);
    return true;
}

bool Camera::uploadLatestFrameToTexture(trussc::Texture& dst) const {
    trussc::Pixels pixels;
    if (!copyLatestFrameToPixels(pixels)) return false;

    if (!dst.isAllocated() ||
        dst.getWidth() != pixels.getWidth() ||
        dst.getHeight() != pixels.getHeight() ||
        dst.getChannels() != pixels.getChannels()) {
        dst.allocate(pixels, trussc::TextureUsage::Dynamic);
    }

    dst.loadData(pixels);
    return true;
}

std::string toString(CameraDevicePosition position) {
    switch (position) {
        case CameraDevicePosition::Unspecified: return "unspecified";
        case CameraDevicePosition::Back: return "back";
        case CameraDevicePosition::Front: return "front";
        case CameraDevicePosition::External: return "external";
    }
    return "unknown";
}

std::string toString(CameraOrientation orientation) {
    switch (orientation) {
        case CameraOrientation::Unspecified: return "unspecified";
        case CameraOrientation::Portrait: return "portrait";
        case CameraOrientation::PortraitUpsideDown: return "portrait upside down";
        case CameraOrientation::LandscapeLeft: return "landscape left";
        case CameraOrientation::LandscapeRight: return "landscape right";
    }
    return "unknown";
}

AudioSession& audioSession() {
    return gAudioSession;
}

void AudioSession::setCategory(const AudioSessionConfig& config, Completion<void> done) {
    detail::platformSetAudioCategory(config, std::move(done));
}

void AudioSession::setActive(bool active, Completion<void> done) {
    detail::platformSetAudioActive(active, std::move(done));
}

void AudioSession::overrideOutputToSpeaker(bool enabled, Completion<void> done) {
    detail::platformOverrideAudioOutputToSpeaker(enabled, std::move(done));
}

AudioRoute AudioSession::currentRoute() const {
    return detail::platformCurrentAudioRoute();
}

void AudioSession::setInterruptionHandler(AudioInterruptionHandler handler) {
    std::lock_guard<std::mutex> lock(gAudioHandlerMutex);
    gAudioInterruptionHandler = std::move(handler);
}

void AudioSession::setRouteChangeHandler(AudioRouteChangeHandler handler) {
    std::lock_guard<std::mutex> lock(gAudioHandlerMutex);
    gAudioRouteChangeHandler = std::move(handler);
}

void AudioSession::clearHandlers() {
    std::lock_guard<std::mutex> lock(gAudioHandlerMutex);
    gAudioInterruptionHandler = nullptr;
    gAudioRouteChangeHandler = nullptr;
}

std::string toString(AudioCategory category) {
    switch (category) {
        case AudioCategory::Ambient: return "ambient";
        case AudioCategory::SoloAmbient: return "solo ambient";
        case AudioCategory::Playback: return "playback";
        case AudioCategory::Record: return "record";
        case AudioCategory::PlayAndRecord: return "play and record";
        case AudioCategory::MultiRoute: return "multi route";
    }
    return "unknown";
}

std::string toString(AudioMode mode) {
    switch (mode) {
        case AudioMode::Default: return "default";
        case AudioMode::VoiceChat: return "voice chat";
        case AudioMode::VideoRecording: return "video recording";
        case AudioMode::Measurement: return "measurement";
    }
    return "unknown";
}

std::string toString(AudioInterruptionType type) {
    switch (type) {
        case AudioInterruptionType::Began: return "began";
        case AudioInterruptionType::Ended: return "ended";
    }
    return "unknown";
}

Haptics& haptics() {
    return gHaptics;
}

bool Haptics::impact(HapticImpactStyle style) {
    return detail::platformHapticImpact(style);
}

bool Haptics::selection() {
    return detail::platformHapticSelection();
}

bool Haptics::notification(HapticNotificationType type) {
    return detail::platformHapticNotification(type);
}

Motion& motion() {
    return gMotion;
}

void Motion::start(const MotionConfig& config, Completion<void> done) {
    detail::platformStartMotion(config, std::move(done));
}

void Motion::stop() {
    detail::platformStopMotion();
}

bool Motion::isRunning() const {
    return detail::platformMotionIsRunning();
}

bool Motion::latest(MotionSample& out) const {
    return detail::platformLatestMotion(out);
}

Notifications& notifications() {
    return gNotifications;
}

void Notifications::settings(Completion<NotificationSettings> done) {
    detail::platformGetNotificationSettings(std::move(done));
}

void Notifications::setCategories(const std::vector<NotificationCategory>& categories,
                                  Completion<void> done) {
    detail::platformSetNotificationCategories(categories, std::move(done));
}

void Notifications::schedule(const LocalNotificationRequest& request, Completion<std::string> done) {
    detail::platformScheduleNotification(request, std::move(done));
}

void Notifications::cancel(const std::string& identifier) {
    detail::platformCancelNotification(identifier);
}

void Notifications::cancelAll() {
    detail::platformCancelAllNotifications();
}

void Notifications::setResponseHandler(NotificationResponseHandler handler) {
    detail::platformSetNotificationResponseHandler(std::move(handler));
}

void Notifications::clearResponseHandler() {
    detail::platformClearNotificationResponseHandler();
}

std::string toString(NotificationSettingState state) {
    switch (state) {
        case NotificationSettingState::NotSupported: return "not supported";
        case NotificationSettingState::Disabled: return "disabled";
        case NotificationSettingState::Enabled: return "enabled";
    }
    return "unknown";
}

Location& location() {
    return gLocation;
}

PermissionState Location::authorizationStatus() const {
    return detail::platformLocationAuthorizationStatus();
}

void Location::requestWhenInUse(Completion<PermissionState> done) {
    detail::platformRequestLocationWhenInUse(std::move(done));
}

void Location::start(const LocationConfig& config, LocationHandler handler) {
    detail::platformStartLocation(config, std::move(handler));
}

void Location::stop() {
    detail::platformStopLocation();
}

bool Location::isRunning() const {
    return detail::platformLocationIsRunning();
}

bool Location::latest(LocationSample& out) const {
    return detail::platformLatestLocation(out);
}

std::string toString(LocationAccuracy accuracy) {
    switch (accuracy) {
        case LocationAccuracy::ThreeKilometers: return "three kilometers";
        case LocationAccuracy::Kilometer: return "kilometer";
        case LocationAccuracy::HundredMeters: return "hundred meters";
        case LocationAccuracy::NearestTenMeters: return "nearest ten meters";
        case LocationAccuracy::Best: return "best";
        case LocationAccuracy::BestForNavigation: return "best for navigation";
    }
    return "unknown";
}

NetworkStatus& networkStatus() {
    return gNetworkStatus;
}

NetworkPath NetworkStatus::current() const {
    return detail::platformCurrentNetworkPath();
}

void NetworkStatus::start(NetworkPathHandler handler) {
    detail::platformStartNetworkStatus(std::move(handler));
}

void NetworkStatus::stop() {
    detail::platformStopNetworkStatus();
}

bool NetworkStatus::isRunning() const {
    return detail::platformNetworkStatusIsRunning();
}

std::string toString(NetworkPathStatus status) {
    switch (status) {
        case NetworkPathStatus::Unknown: return "unknown";
        case NetworkPathStatus::Satisfied: return "satisfied";
        case NetworkPathStatus::Unsatisfied: return "unsatisfied";
        case NetworkPathStatus::RequiresConnection: return "requires connection";
    }
    return "unknown";
}

std::string toString(NetworkInterface interfaceType) {
    switch (interfaceType) {
        case NetworkInterface::WiFi: return "wifi";
        case NetworkInterface::Cellular: return "cellular";
        case NetworkInterface::WiredEthernet: return "wired ethernet";
        case NetworkInterface::Loopback: return "loopback";
        case NetworkInterface::Other: return "other";
    }
    return "unknown";
}

BackgroundTasks& backgroundTasks() {
    return gBackgroundTasks;
}

void BackgroundTasks::registerHandler(const BackgroundTaskRegistration& registration,
                                      BackgroundTaskHandler handler,
                                      Completion<void> done) {
    detail::platformRegisterBackgroundTask(registration, std::move(handler), std::move(done));
}

void BackgroundTasks::schedule(const BackgroundTaskRequest& request, Completion<void> done) {
    detail::platformScheduleBackgroundTask(request, std::move(done));
}

void BackgroundTasks::cancel(const std::string& identifier) {
    detail::platformCancelBackgroundTask(identifier);
}

void BackgroundTasks::cancelAll() {
    detail::platformCancelAllBackgroundTasks();
}

std::string toString(BackgroundTaskKind kind) {
    switch (kind) {
        case BackgroundTaskKind::AppRefresh: return "app refresh";
        case BackgroundTaskKind::Processing: return "processing";
    }
    return "unknown";
}

BackgroundDownloads& backgroundDownloads() {
    return gBackgroundDownloads;
}

void BackgroundDownloads::download(const BackgroundDownloadRequest& request,
                                   Completion<BackgroundDownloadResult> done,
                                   BackgroundDownloadProgressHandler progress) {
    detail::platformStartBackgroundDownload(request, std::move(done), std::move(progress));
}

OperationHandle BackgroundDownloads::downloadCancellable(const BackgroundDownloadRequest& request,
                                                         Completion<BackgroundDownloadResult> done,
                                                         BackgroundDownloadProgressHandler progress) {
    OperationHandle operation = operations().create("BackgroundDownloads.download", [identifier = request.identifier]() {
        if (!identifier.empty()) detail::platformCancelBackgroundDownload(identifier);
    });
    BackgroundDownloadProgressHandler wrappedProgress;
    if (progress) {
        wrappedProgress = [operation, progress = std::move(progress)](const BackgroundDownloadProgress& value) mutable {
            if (!operation.cancelled()) progress(value);
        };
    }
    detail::platformStartBackgroundDownload(request,
                                            cancellableCompletion(operation, std::move(done)),
                                            std::move(wrappedProgress));
    return operation;
}

void BackgroundDownloads::cancel(const std::string& identifier) {
    detail::platformCancelBackgroundDownload(identifier);
}

std::vector<BackgroundDownloadRequest> BackgroundDownloads::pendingRequests() const {
    return detail::platformPendingBackgroundDownloads();
}

Keychain& keychain() {
    return gKeychain;
}

Result<void> Keychain::set(const KeychainItem& item) {
    return detail::platformKeychainSet(item);
}

Result<std::vector<std::uint8_t>> Keychain::get(const std::string& service,
                                                const std::string& account) const {
    return detail::platformKeychainGet(service, account);
}

Result<std::string> Keychain::getString(const std::string& service,
                                        const std::string& account) const {
    Result<std::vector<std::uint8_t>> result = get(service, account);
    if (!result.ok) return Result<std::string>::failure(std::move(result.error));
    return Result<std::string>::success(std::string(result.value.begin(), result.value.end()));
}

Result<void> Keychain::setString(const std::string& service,
                                 const std::string& account,
                                 const std::string& value,
                                 KeychainAccessibility accessibility) {
    KeychainItem item;
    item.service = service;
    item.account = account;
    item.data.assign(value.begin(), value.end());
    item.accessibility = accessibility;
    return set(item);
}

Result<void> Keychain::remove(const std::string& service, const std::string& account) {
    return detail::platformKeychainRemove(service, account);
}

LocalAuthentication& localAuthentication() {
    return gLocalAuthentication;
}

AuthenticationAvailability LocalAuthentication::availability(AuthenticationPolicy policy) const {
    return detail::platformAuthenticationAvailability(policy);
}

void LocalAuthentication::evaluate(const AuthenticationRequest& request,
                                   Completion<AuthenticationResult> done) {
    detail::platformEvaluateAuthentication(request, std::move(done));
}

std::string toString(KeychainAccessibility accessibility) {
    switch (accessibility) {
        case KeychainAccessibility::WhenUnlocked: return "when unlocked";
        case KeychainAccessibility::AfterFirstUnlock: return "after first unlock";
    }
    return "unknown";
}

std::string toString(AuthenticationPolicy policy) {
    switch (policy) {
        case AuthenticationPolicy::DeviceOwnerAuthentication: return "device owner authentication";
        case AuthenticationPolicy::DeviceOwnerAuthenticationWithBiometrics: return "device owner authentication with biometrics";
    }
    return "unknown";
}

Web& web() {
    return gWeb;
}

void Web::openSafari(const SafariRequest& request, Completion<void> done) {
    detail::platformOpenSafari(request, std::move(done));
}

ExternalDisplay& externalDisplay() {
    return gExternalDisplay;
}

std::vector<ExternalScreenInfo> ExternalDisplay::screens() const {
    return detail::platformExternalScreens();
}

bool ExternalDisplay::hasExternalScreen() const {
    return detail::platformExternalScreens().size() > 1;
}

void ExternalDisplay::setChangeHandler(ExternalDisplayChangeHandler handler) {
    {
        std::lock_guard<std::mutex> lock(gExternalDisplayHandlerMutex);
        gExternalDisplayChangeHandler = std::move(handler);
    }
    detail::platformStartExternalDisplayObserving();
}

void ExternalDisplay::clearChangeHandler() {
    {
        std::lock_guard<std::mutex> lock(gExternalDisplayHandlerMutex);
        gExternalDisplayChangeHandler = nullptr;
    }
    detail::platformStopExternalDisplayObserving();
}

void ExternalDisplay::show(const ExternalDisplayRequest& request,
                           Completion<ExternalDisplayPresentation> done) {
    detail::platformShowExternalDisplay(request, std::move(done));
}

void ExternalDisplay::dismiss(const std::string& screenIdentifier) {
    detail::platformDismissExternalDisplay(screenIdentifier);
}

void ExternalDisplay::dismissAll() {
    detail::platformDismissAllExternalDisplays();
}

BluetoothLE& bluetoothLE() {
    return gBluetoothLE;
}

BluetoothState BluetoothLE::state() const {
    return detail::platformBluetoothState();
}

void BluetoothLE::startScan(const BLEScanRequest& request, BLEScanHandler handler) {
    detail::platformStartBLEScan(request, std::move(handler));
}

OperationHandle BluetoothLE::startScanCancellable(const BLEScanRequest& request, BLEScanHandler handler) {
    OperationHandle operation = operations().create("BluetoothLE.startScan", []() {
        detail::platformStopBLEScan();
    });
    detail::platformStartBLEScan(request, [operation, handler = std::move(handler)](const BLEPeripheralInfo& info) mutable {
        if (!operation.cancelled() && handler) handler(info);
    });
    return operation;
}

void BluetoothLE::stopScan() {
    detail::platformStopBLEScan();
}

void BluetoothLE::connect(const std::string& peripheralIdentifier, Completion<void> done) {
    detail::platformBLEConnect(peripheralIdentifier, std::move(done));
}

OperationHandle BluetoothLE::connectCancellable(const std::string& peripheralIdentifier, Completion<void> done) {
    OperationHandle operation = operations().create("BluetoothLE.connect", [peripheralIdentifier]() {
        detail::platformBLEDisconnect(peripheralIdentifier);
    });
    detail::platformBLEConnect(peripheralIdentifier, cancellableCompletion(operation, std::move(done)));
    return operation;
}

void BluetoothLE::disconnect(const std::string& peripheralIdentifier) {
    detail::platformBLEDisconnect(peripheralIdentifier);
}

void BluetoothLE::read(const BLECharacteristicRef& characteristic, BLEValueHandler done) {
    detail::platformBLERead(characteristic, std::move(done));
}

OperationHandle BluetoothLE::readCancellable(const BLECharacteristicRef& characteristic, BLEValueHandler done) {
    OperationHandle operation = operations().create("BluetoothLE.read");
    detail::platformBLERead(characteristic, cancellableCompletion(operation, std::move(done)));
    return operation;
}

void BluetoothLE::write(const BLEWriteRequest& request, Completion<void> done) {
    detail::platformBLEWrite(request, std::move(done));
}

OperationHandle BluetoothLE::writeCancellable(const BLEWriteRequest& request, Completion<void> done) {
    OperationHandle operation = operations().create("BluetoothLE.write");
    detail::platformBLEWrite(request, cancellableCompletion(operation, std::move(done)));
    return operation;
}

void BluetoothLE::setNotify(const BLECharacteristicRef& characteristic, bool enabled, BLEValueHandler handler) {
    detail::platformBLESetNotify(characteristic, enabled, std::move(handler));
}

OperationHandle BluetoothLE::setNotifyCancellable(const BLECharacteristicRef& characteristic,
                                                  bool enabled,
                                                  BLEValueHandler handler) {
    OperationHandle operation = operations().create("BluetoothLE.setNotify", [characteristic]() {
        detail::platformBLESetNotify(characteristic, false, nullptr);
    });
    detail::platformBLESetNotify(characteristic,
                                 enabled,
                                 [operation, handler = std::move(handler)](Result<BLECharacteristicValue> result) mutable {
        if (!operation.cancelled() && handler) handler(std::move(result));
    });
    return operation;
}

std::string toString(BluetoothState state) {
    switch (state) {
        case BluetoothState::Unknown: return "unknown";
        case BluetoothState::Unsupported: return "unsupported";
        case BluetoothState::Unauthorized: return "unauthorized";
        case BluetoothState::PoweredOff: return "powered off";
        case BluetoothState::PoweredOn: return "powered on";
    }
    return "unknown";
}

Multipeer& multipeer() {
    return gMultipeer;
}

void Multipeer::start(const MultipeerConfig& config, Completion<void> done) {
    detail::platformStartMultipeer(config, std::move(done));
}

void Multipeer::stop() {
    detail::platformStopMultipeer();
}

std::vector<MultipeerPeer> Multipeer::peers() const {
    return detail::platformMultipeerPeers();
}

void Multipeer::send(const std::vector<std::uint8_t>& data, Completion<void> done) {
    detail::platformMultipeerSend(data, std::move(done));
}

void Multipeer::setPeerHandler(MultipeerPeerHandler handler) {
    detail::platformSetMultipeerPeerHandler(std::move(handler));
}

void Multipeer::setMessageHandler(MultipeerMessageHandler handler) {
    detail::platformSetMultipeerMessageHandler(std::move(handler));
}

GameController& gameController() {
    return gGameController;
}

std::vector<std::string> GameController::connectedControllerNames() const {
    return detail::platformConnectedGameControllerNames();
}

bool GameController::latest(GameControllerState& out) const {
    return detail::platformLatestGameController(out);
}

PencilCanvas& pencilCanvas() {
    return gPencilCanvas;
}

void PencilCanvas::present(const PencilCanvasRequest& request, Completion<void> done) {
    detail::platformPresentPencilCanvas(request, std::move(done));
}

void PencilCanvas::dismiss() {
    detail::platformDismissPencilCanvas();
}

Result<PencilDrawingData> PencilCanvas::capture() const {
    return detail::platformCapturePencilDrawing();
}

void PencilCanvas::clear() {
    detail::platformClearPencilCanvas();
}

StoreKit& storeKit() {
    return gStoreKit;
}

bool StoreKit::canMakePayments() const {
    return detail::platformStoreCanMakePayments();
}

void StoreKit::requestProducts(const std::vector<std::string>& productIdentifiers,
                               StoreProductsHandler done) {
    detail::platformRequestStoreProducts(productIdentifiers, std::move(done));
}

void StoreKit::purchase(const std::string& productIdentifier,
                        Completion<StorePurchaseResult> done) {
    detail::platformPurchaseStoreProduct(productIdentifier, std::move(done));
}

void StoreKit::restorePurchases(Completion<std::vector<StoreTransactionUpdate>> done) {
    detail::platformRestoreStorePurchases(std::move(done));
}

void StoreKit::setTransactionUpdateHandler(StoreTransactionUpdateHandler handler) {
    detail::platformSetStoreTransactionUpdateHandler(std::move(handler));
}

void StoreKit::clearTransactionUpdateHandler() {
    detail::platformClearStoreTransactionUpdateHandler();
}

std::string toString(StoreTransactionState state) {
    switch (state) {
        case StoreTransactionState::Unknown: return "unknown";
        case StoreTransactionState::Purchasing: return "purchasing";
        case StoreTransactionState::Purchased: return "purchased";
        case StoreTransactionState::Restored: return "restored";
        case StoreTransactionState::Failed: return "failed";
        case StoreTransactionState::Deferred: return "deferred";
        case StoreTransactionState::Revoked: return "revoked";
    }
    return "unknown";
}

ContactsUI& contactsUI() {
    return gContactsUI;
}

void ContactsUI::pickContact(Completion<PickedContact> done) {
    detail::platformPickContact(std::move(done));
}

ARKitBridge& arKit() {
    return gARKitBridge;
}

bool ARKitBridge::isWorldTrackingSupported() const {
    return detail::platformARWorldTrackingSupported();
}

void ARKitBridge::start(const ARSessionConfig& config, Completion<void> done) {
    detail::platformStartARSession(config, std::move(done));
}

void ARKitBridge::stop() {
    detail::platformStopARSession();
}

bool ARKitBridge::latestFrame(ARFrameInfo& out) const {
    return detail::platformLatestARFrame(out);
}

VisionBridge& vision() {
    return gVisionBridge;
}

void VisionBridge::detectRectangles(const std::filesystem::path& imagePath,
                                    Completion<std::vector<VisionRectangle>> done) {
    detail::platformDetectVisionRectangles(imagePath, std::move(done));
}

CoreMLBridge& coreML() {
    return gCoreMLBridge;
}

Result<CoreMLModelInfo> CoreMLBridge::inspectModel(const std::filesystem::path& compiledModelPath) const {
    return detail::platformInspectCoreMLModel(compiledModelPath);
}

namespace detail {

void dispatchAudioInterruption(const AudioInterruption& interruption) {
    eventQueue().post([interruption]() {
        AudioInterruptionHandler handler;
        {
            std::lock_guard<std::mutex> lock(gAudioHandlerMutex);
            handler = gAudioInterruptionHandler;
        }
        if (handler) handler(interruption);
    });
}

void dispatchAudioRouteChange(const AudioRouteChange& routeChange) {
    eventQueue().post([routeChange]() {
        AudioRouteChangeHandler handler;
        {
            std::lock_guard<std::mutex> lock(gAudioHandlerMutex);
            handler = gAudioRouteChangeHandler;
        }
        if (handler) handler(routeChange);
    });
}

void dispatchExternalDisplaysChanged(const std::vector<ExternalScreenInfo>& screens) {
    eventQueue().post([screens]() {
        ExternalDisplayChangeHandler handler;
        {
            std::lock_guard<std::mutex> lock(gExternalDisplayHandlerMutex);
            handler = gExternalDisplayChangeHandler;
        }
        if (handler) handler(screens);
    });
}

} // namespace detail

} // namespace tcx::ios
