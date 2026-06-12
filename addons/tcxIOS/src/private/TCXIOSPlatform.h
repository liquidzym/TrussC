#pragma once

#include "tcx/ios/App.h"
#include "tcx/ios/ARVisionCoreML.h"
#include "tcx/ios/AudioSession.h"
#include "tcx/ios/BackgroundDownloads.h"
#include "tcx/ios/BackgroundTasks.h"
#include "tcx/ios/Bluetooth.h"
#include "tcx/ios/Camera.h"
#include "tcx/ios/ContactsUI.h"
#include "tcx/ios/ExternalDisplay.h"
#include "tcx/ios/Files.h"
#include "tcx/ios/GameController.h"
#include "tcx/ios/Haptics.h"
#include "tcx/ios/Location.h"
#include "tcx/ios/Motion.h"
#include "tcx/ios/Multipeer.h"
#include "tcx/ios/NativeUI.h"
#include "tcx/ios/NetworkStatus.h"
#include "tcx/ios/Notifications.h"
#include "tcx/ios/Operations.h"
#include "tcx/ios/PencilKit.h"
#include "tcx/ios/Permissions.h"
#include "tcx/ios/Photos.h"
#include "tcx/ios/Scene.h"
#include "tcx/ios/Security.h"
#include "tcx/ios/StoreKit.h"
#include "tcx/ios/Web.h"
#include "tcx/ios/Logger.h"

namespace tcx::ios::detail {

AppState platformAppState();
ScreenInfo platformMainScreen();
SafeAreaInsets platformSafeAreaInsets();
Orientation platformOrientation();
DeviceInfo platformDeviceInfo();

void platformShowAlert(const AlertRequest& request, Completion<AlertResult> done);
void platformShare(const ShareRequest& request, Completion<ShareResult> done);
void platformOpenSettings();
void platformOpenURL(const std::string& url, Completion<void> done);

PermissionState platformPermissionStatus(Permission permission);
void platformRequestPermission(Permission permission, Completion<PermissionState> done);
PermissionState platformBluetoothPermissionStatus();
PermissionState platformMotionPermissionStatus();
PermissionState platformContactsPermissionStatus();
void platformRequestContactsPermission(Completion<PermissionState> done);

std::filesystem::path platformAppDirectoryPath(AppDirectory directory);
void platformImportFiles(const ImportFileRequest& request, Completion<std::vector<PickedFile>> done);
void platformExportFile(const ExportFileRequest& request, Completion<void> done);
void platformStopAccessingFile(const PickedFile& file);
void platformPickPhotos(const PhotoPickerRequest& request, Completion<std::vector<PickedPhoto>> done);
void platformSavePhoto(const PhotoSaveRequest& request, Completion<void> done);

void platformStartCamera(const CameraConfig& config, Completion<void> done);
void platformStopCamera();
bool platformCameraIsRunning();
bool platformLatestCameraFrame(CameraFrame& out);
bool platformLatestCameraFrameView(CameraFrameView& out);
std::vector<CameraDeviceInfo> platformAvailableCameraDevices();

void platformSetAudioCategory(const AudioSessionConfig& config, Completion<void> done);
void platformSetAudioActive(bool active, Completion<void> done);
void platformOverrideAudioOutputToSpeaker(bool enabled, Completion<void> done);
AudioRoute platformCurrentAudioRoute();
void dispatchAudioInterruption(const AudioInterruption& interruption);
void dispatchAudioRouteChange(const AudioRouteChange& routeChange);

bool platformHapticImpact(HapticImpactStyle style);
bool platformHapticSelection();
bool platformHapticNotification(HapticNotificationType type);

void platformStartMotion(const MotionConfig& config, Completion<void> done);
void platformStopMotion();
bool platformMotionIsRunning();
bool platformLatestMotion(MotionSample& out);

void platformGetNotificationSettings(Completion<NotificationSettings> done);
void platformSetNotificationCategories(const std::vector<NotificationCategory>& categories, Completion<void> done);
void platformScheduleNotification(const LocalNotificationRequest& request, Completion<std::string> done);
void platformCancelNotification(const std::string& identifier);
void platformCancelAllNotifications();
void platformSetNotificationResponseHandler(NotificationResponseHandler handler);
void platformClearNotificationResponseHandler();

PermissionState platformLocationAuthorizationStatus();
void platformRequestLocationWhenInUse(Completion<PermissionState> done);
void platformStartLocation(const LocationConfig& config, LocationHandler handler);
void platformStopLocation();
bool platformLocationIsRunning();
bool platformLatestLocation(LocationSample& out);

NetworkPath platformCurrentNetworkPath();
void platformStartNetworkStatus(NetworkPathHandler handler);
void platformStopNetworkStatus();
bool platformNetworkStatusIsRunning();

void platformRegisterBackgroundTask(const BackgroundTaskRegistration& registration,
                                    BackgroundTaskHandler handler,
                                    Completion<void> done);
void platformScheduleBackgroundTask(const BackgroundTaskRequest& request, Completion<void> done);
void platformCancelBackgroundTask(const std::string& identifier);
void platformCancelAllBackgroundTasks();

void platformStartBackgroundDownload(const BackgroundDownloadRequest& request,
                                     Completion<BackgroundDownloadResult> done,
                                     BackgroundDownloadProgressHandler progress);
void platformCancelBackgroundDownload(const std::string& identifier);
std::vector<BackgroundDownloadRequest> platformPendingBackgroundDownloads();

Result<void> platformKeychainSet(const KeychainItem& item);
Result<std::vector<std::uint8_t>> platformKeychainGet(const std::string& service,
                                                      const std::string& account);
Result<void> platformKeychainRemove(const std::string& service, const std::string& account);
AuthenticationAvailability platformAuthenticationAvailability(AuthenticationPolicy policy);
void platformEvaluateAuthentication(const AuthenticationRequest& request,
                                    Completion<AuthenticationResult> done);

void platformOpenSafari(const SafariRequest& request, Completion<void> done);

std::vector<ExternalScreenInfo> platformExternalScreens();
void platformStartExternalDisplayObserving();
void platformStopExternalDisplayObserving();
void platformShowExternalDisplay(const ExternalDisplayRequest& request,
                                 Completion<ExternalDisplayPresentation> done);
void platformDismissExternalDisplay(const std::string& screenIdentifier);
void platformDismissAllExternalDisplays();
void dispatchExternalDisplaysChanged(const std::vector<ExternalScreenInfo>& screens);

BluetoothState platformBluetoothState();
void platformStartBLEScan(const BLEScanRequest& request, BLEScanHandler handler);
void platformStopBLEScan();
void platformBLEConnect(const std::string& peripheralIdentifier, Completion<void> done);
void platformBLEDisconnect(const std::string& peripheralIdentifier);
void platformBLERead(const BLECharacteristicRef& characteristic, BLEValueHandler done);
void platformBLEWrite(const BLEWriteRequest& request, Completion<void> done);
void platformBLESetNotify(const BLECharacteristicRef& characteristic, bool enabled, BLEValueHandler handler);

void platformStartMultipeer(const MultipeerConfig& config, Completion<void> done);
void platformStopMultipeer();
std::vector<MultipeerPeer> platformMultipeerPeers();
void platformMultipeerSend(const std::vector<std::uint8_t>& data, Completion<void> done);
void platformSetMultipeerPeerHandler(MultipeerPeerHandler handler);
void platformSetMultipeerMessageHandler(MultipeerMessageHandler handler);

std::vector<std::string> platformConnectedGameControllerNames();
bool platformLatestGameController(GameControllerState& out);

void platformPresentPencilCanvas(const PencilCanvasRequest& request, Completion<void> done);
void platformDismissPencilCanvas();
Result<PencilDrawingData> platformCapturePencilDrawing();
void platformClearPencilCanvas();

bool platformStoreCanMakePayments();
void platformRequestStoreProducts(const std::vector<std::string>& productIdentifiers,
                                  StoreProductsHandler done);
void platformPurchaseStoreProduct(const std::string& productIdentifier,
                                  Completion<StorePurchaseResult> done);
void platformRestoreStorePurchases(Completion<std::vector<StoreTransactionUpdate>> done);
void platformSetStoreTransactionUpdateHandler(StoreTransactionUpdateHandler handler);
void platformClearStoreTransactionUpdateHandler();

void platformPickContact(Completion<PickedContact> done);

bool platformARWorldTrackingSupported();
void platformStartARSession(const ARSessionConfig& config, Completion<void> done);
void platformStopARSession();
bool platformLatestARFrame(ARFrameInfo& out);
void platformDetectVisionRectangles(const std::filesystem::path& imagePath,
                                    Completion<std::vector<VisionRectangle>> done);
void platformMakeVisionMask(const VisionMaskRequest& request, Completion<VisionMaskResult> done);
Result<CoreMLModelInfo> platformInspectCoreMLModel(const std::filesystem::path& compiledModelPath);

} // namespace tcx::ios::detail
