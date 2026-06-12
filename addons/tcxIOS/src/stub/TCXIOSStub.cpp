#include "TCXIOSPlatform.h"
#include "tcx/ios/EventQueue.h"

#include <utility>

namespace tcx::ios::detail {
namespace {

template <typename T>
void finishUnavailable(Completion<T> done, const std::string& feature) {
    if (!done) return;
    eventQueue().post([done = std::move(done), feature]() mutable {
        done(Result<T>::failure(unavailableError(feature)));
    });
}

void finishUnavailableVoid(Completion<void> done, const std::string& feature) {
    if (!done) return;
    eventQueue().post([done = std::move(done), feature]() mutable {
        done(Result<void>::failure(unavailableError(feature)));
    });
}

template <typename T>
Result<T> unavailableResult(const std::string& feature) {
    return Result<T>::failure(unavailableError(feature));
}

} // namespace

AppState platformAppState() {
    return AppState::Active;
}

ScreenInfo platformMainScreen() {
    return {};
}

SafeAreaInsets platformSafeAreaInsets() {
    return {};
}

Orientation platformOrientation() {
    return Orientation::Unknown;
}

DeviceInfo platformDeviceInfo() {
    return {
        "non-ios",
        "Non-iOS stub",
        "",
        "",
        false
    };
}

void platformShowAlert(const AlertRequest&, Completion<AlertResult> done) {
    finishUnavailable(std::move(done), "NativeUI.showAlert");
}

void platformShare(const ShareRequest&, Completion<ShareResult> done) {
    finishUnavailable(std::move(done), "NativeUI.share");
}

void platformOpenSettings() {}

void platformOpenURL(const std::string&, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "NativeUI.openURL");
}

PermissionState platformPermissionStatus(Permission) {
    return PermissionState::Unknown;
}

void platformRequestPermission(Permission permission, Completion<PermissionState> done) {
    finishUnavailable(std::move(done), "Permissions.request(" + toString(permission) + ")");
}

PermissionState platformBluetoothPermissionStatus() {
    return PermissionState::Unknown;
}

PermissionState platformMotionPermissionStatus() {
    return PermissionState::Unknown;
}

PermissionState platformContactsPermissionStatus() {
    return PermissionState::Unknown;
}

void platformRequestContactsPermission(Completion<PermissionState> done) {
    finishUnavailable(std::move(done), "Permissions.request(" + toString(Permission::Contacts) + ")");
}

std::filesystem::path platformAppDirectoryPath(AppDirectory directory) {
    std::filesystem::path base = std::filesystem::temp_directory_path() / "tcxIOS";
    switch (directory) {
        case AppDirectory::Documents: return base / "Documents";
        case AppDirectory::Caches: return base / "Caches";
        case AppDirectory::Temporary: return base / "Temporary";
        case AppDirectory::ApplicationSupport: return base / "Application Support";
    }
    return base;
}

void platformImportFiles(const ImportFileRequest&, Completion<std::vector<PickedFile>> done) {
    finishUnavailable(std::move(done), "Files.importFiles");
}

void platformExportFile(const ExportFileRequest&, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "Files.exportFile");
}

void platformStopAccessingFile(const PickedFile&) {}

void platformPickPhotos(const PhotoPickerRequest&, Completion<std::vector<PickedPhoto>> done) {
    finishUnavailable(std::move(done), "Photos.pickPhotos");
}

void platformSavePhoto(const PhotoSaveRequest&, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "Photos.save");
}

void platformStartCamera(const CameraConfig&, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "Camera.start");
}

void platformStopCamera() {}

bool platformCameraIsRunning() {
    return false;
}

bool platformLatestCameraFrame(CameraFrame&) {
    return false;
}

bool platformLatestCameraFrameView(CameraFrameView&) {
    return false;
}

std::vector<CameraDeviceInfo> platformAvailableCameraDevices() {
    return {};
}

void platformSetAudioCategory(const AudioSessionConfig&, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "AudioSession.setCategory");
}

void platformSetAudioActive(bool, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "AudioSession.setActive");
}

void platformOverrideAudioOutputToSpeaker(bool, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "AudioSession.overrideOutputToSpeaker");
}

AudioRoute platformCurrentAudioRoute() {
    return {};
}

bool platformHapticImpact(HapticImpactStyle) {
    return false;
}

bool platformHapticSelection() {
    return false;
}

bool platformHapticNotification(HapticNotificationType) {
    return false;
}

void platformStartMotion(const MotionConfig&, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "Motion.start");
}

void platformStopMotion() {}

bool platformMotionIsRunning() {
    return false;
}

bool platformLatestMotion(MotionSample&) {
    return false;
}

void platformGetNotificationSettings(Completion<NotificationSettings> done) {
    finishUnavailable(std::move(done), "Notifications.settings");
}

void platformSetNotificationCategories(const std::vector<NotificationCategory>&, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "Notifications.setCategories");
}

void platformScheduleNotification(const LocalNotificationRequest&, Completion<std::string> done) {
    finishUnavailable(std::move(done), "Notifications.schedule");
}

void platformCancelNotification(const std::string&) {}

void platformCancelAllNotifications() {}

void platformSetNotificationResponseHandler(NotificationResponseHandler) {}

void platformClearNotificationResponseHandler() {}

PermissionState platformLocationAuthorizationStatus() {
    return PermissionState::Unknown;
}

void platformRequestLocationWhenInUse(Completion<PermissionState> done) {
    finishUnavailable(std::move(done), "Location.requestWhenInUse");
}

void platformStartLocation(const LocationConfig&, LocationHandler handler) {
    if (!handler) return;
    eventQueue().post([handler = std::move(handler)]() mutable {
        handler(Result<LocationSample>::failure(unavailableError("Location.start")));
    });
}

void platformStopLocation() {}

bool platformLocationIsRunning() {
    return false;
}

bool platformLatestLocation(LocationSample&) {
    return false;
}

NetworkPath platformCurrentNetworkPath() {
    return {};
}

void platformStartNetworkStatus(NetworkPathHandler handler) {
    if (!handler) return;
    eventQueue().post([handler = std::move(handler)]() mutable {
        handler({});
    });
}

void platformStopNetworkStatus() {}

bool platformNetworkStatusIsRunning() {
    return false;
}

void platformRegisterBackgroundTask(const BackgroundTaskRegistration&,
                                    BackgroundTaskHandler,
                                    Completion<void> done) {
    finishUnavailableVoid(std::move(done), "BackgroundTasks.registerHandler");
}

void platformScheduleBackgroundTask(const BackgroundTaskRequest&, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "BackgroundTasks.schedule");
}

void platformCancelBackgroundTask(const std::string&) {}

void platformCancelAllBackgroundTasks() {}

void platformStartBackgroundDownload(const BackgroundDownloadRequest&,
                                     Completion<BackgroundDownloadResult> done,
                                     BackgroundDownloadProgressHandler) {
    finishUnavailable(std::move(done), "BackgroundDownloads.download");
}

void platformCancelBackgroundDownload(const std::string&) {}

std::vector<BackgroundDownloadRequest> platformPendingBackgroundDownloads() {
    return {};
}

Result<void> platformKeychainSet(const KeychainItem&) {
    return Result<void>::failure(unavailableError("Keychain.set"));
}

Result<std::vector<std::uint8_t>> platformKeychainGet(const std::string&, const std::string&) {
    return unavailableResult<std::vector<std::uint8_t>>("Keychain.get");
}

Result<void> platformKeychainRemove(const std::string&, const std::string&) {
    return Result<void>::failure(unavailableError("Keychain.remove"));
}

AuthenticationAvailability platformAuthenticationAvailability(AuthenticationPolicy) {
    AuthenticationAvailability availability;
    availability.available = false;
    availability.error = unavailableError("LocalAuthentication.availability");
    return availability;
}

void platformEvaluateAuthentication(const AuthenticationRequest&,
                                    Completion<AuthenticationResult> done) {
    finishUnavailable(std::move(done), "LocalAuthentication.evaluate");
}

void platformOpenSafari(const SafariRequest&, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "Web.openSafari");
}

std::vector<ExternalScreenInfo> platformExternalScreens() {
    return {};
}

void platformStartExternalDisplayObserving() {}

void platformStopExternalDisplayObserving() {}

void platformShowExternalDisplay(const ExternalDisplayRequest&,
                                 Completion<ExternalDisplayPresentation> done) {
    finishUnavailable(std::move(done), "ExternalDisplay.show");
}

void platformDismissExternalDisplay(const std::string&) {}

void platformDismissAllExternalDisplays() {}

BluetoothState platformBluetoothState() {
    return BluetoothState::Unsupported;
}

void platformStartBLEScan(const BLEScanRequest&, BLEScanHandler) {}

void platformStopBLEScan() {}

void platformBLEConnect(const std::string&, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "BluetoothLE.connect");
}

void platformBLEDisconnect(const std::string&) {}

void platformBLERead(const BLECharacteristicRef&, BLEValueHandler done) {
    finishUnavailable(std::move(done), "BluetoothLE.read");
}

void platformBLEWrite(const BLEWriteRequest&, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "BluetoothLE.write");
}

void platformBLESetNotify(const BLECharacteristicRef&, bool, BLEValueHandler handler) {
    if (!handler) return;
    eventQueue().post([handler = std::move(handler)]() mutable {
        handler(Result<BLECharacteristicValue>::failure(unavailableError("BluetoothLE.setNotify")));
    });
}

void platformStartMultipeer(const MultipeerConfig&, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "Multipeer.start");
}

void platformStopMultipeer() {}

std::vector<MultipeerPeer> platformMultipeerPeers() {
    return {};
}

void platformMultipeerSend(const std::vector<std::uint8_t>&, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "Multipeer.send");
}

void platformSetMultipeerPeerHandler(MultipeerPeerHandler) {}

void platformSetMultipeerMessageHandler(MultipeerMessageHandler) {}

std::vector<std::string> platformConnectedGameControllerNames() {
    return {};
}

bool platformLatestGameController(GameControllerState&) {
    return false;
}

void platformPresentPencilCanvas(const PencilCanvasRequest&, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "PencilCanvas.present");
}

void platformDismissPencilCanvas() {}

Result<PencilDrawingData> platformCapturePencilDrawing() {
    return unavailableResult<PencilDrawingData>("PencilCanvas.capture");
}

void platformClearPencilCanvas() {}

bool platformStoreCanMakePayments() {
    return false;
}

void platformRequestStoreProducts(const std::vector<std::string>&,
                                  StoreProductsHandler done) {
    finishUnavailable(std::move(done), "StoreKit.requestProducts");
}

void platformPurchaseStoreProduct(const std::string&, Completion<StorePurchaseResult> done) {
    finishUnavailable(std::move(done), "StoreKit.purchase");
}

void platformRestoreStorePurchases(Completion<std::vector<StoreTransactionUpdate>> done) {
    finishUnavailable(std::move(done), "StoreKit.restorePurchases");
}

void platformSetStoreTransactionUpdateHandler(StoreTransactionUpdateHandler) {}

void platformClearStoreTransactionUpdateHandler() {}

void platformPickContact(Completion<PickedContact> done) {
    finishUnavailable(std::move(done), "ContactsUI.pickContact");
}

bool platformARWorldTrackingSupported() {
    return false;
}

void platformStartARSession(const ARSessionConfig&, Completion<void> done) {
    finishUnavailableVoid(std::move(done), "ARKit.start");
}

void platformStopARSession() {}

bool platformLatestARFrame(ARFrameInfo&) {
    return false;
}

void platformDetectVisionRectangles(const std::filesystem::path&,
                                    Completion<std::vector<VisionRectangle>> done) {
    finishUnavailable(std::move(done), "Vision.detectRectangles");
}

void platformMakeVisionMask(const VisionMaskRequest&, Completion<VisionMaskResult> done) {
    finishUnavailable(std::move(done), "Vision.makeMask");
}

Result<CoreMLModelInfo> platformInspectCoreMLModel(const std::filesystem::path&) {
    return unavailableResult<CoreMLModelInfo>("CoreML.inspectModel");
}

} // namespace tcx::ios::detail
