#include "TCXIOSBridgeSupport.h"

#import <AVFoundation/AVFoundation.h>
#import <BackgroundTasks/BackgroundTasks.h>
#import <CoreLocation/CoreLocation.h>
#import <CoreMotion/CoreMotion.h>
#import <Network/Network.h>
#import <Photos/Photos.h>
#import <UserNotifications/UserNotifications.h>

#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>


namespace {

dispatch_queue_t TCXIOSNetworkQueue() {
    static dispatch_queue_t queue = dispatch_queue_create("org.trussc.tcxios.network", DISPATCH_QUEUE_SERIAL);
    return queue;
}

std::mutex gTCXIOSLocationMutex;
tcx::ios::LocationSample gTCXIOSLatestLocation;
tcx::ios::LocationHandler gTCXIOSLocationHandler;
tcx::ios::Completion<tcx::ios::PermissionState> gTCXIOSLocationPermissionCompletion;
std::atomic_bool gTCXIOSLocationRunning{false};
CLLocationManager* gTCXIOSLocationManager = nil;
id gTCXIOSLocationDelegate = nil;

std::mutex gTCXIOSNetworkMutex;
tcx::ios::NetworkPath gTCXIOSLatestNetworkPath;
tcx::ios::NetworkPathHandler gTCXIOSNetworkHandler;
std::atomic_bool gTCXIOSNetworkRunning{false};
nw_path_monitor_t gTCXIOSNetworkMonitor = nil;

std::mutex gTCXIOSBackgroundTaskMutex;
std::map<std::string, tcx::ios::BackgroundTaskHandler> gTCXIOSBackgroundTaskHandlers;
std::map<std::string, tcx::ios::BackgroundTaskKind> gTCXIOSBackgroundTaskKinds;

tcx::ios::PermissionState TCXIOSMapCLAuthorization(CLAuthorizationStatus status) {
    switch (status) {
        case kCLAuthorizationStatusNotDetermined: return tcx::ios::PermissionState::NotDetermined;
        case kCLAuthorizationStatusRestricted: return tcx::ios::PermissionState::Restricted;
        case kCLAuthorizationStatusDenied: return tcx::ios::PermissionState::Denied;
        case kCLAuthorizationStatusAuthorizedAlways:
        case kCLAuthorizationStatusAuthorizedWhenInUse: return tcx::ios::PermissionState::Authorized;
    }
    return tcx::ios::PermissionState::Unknown;
}

tcx::ios::LocationSample TCXIOSMapLocation(CLLocation* location) {
    tcx::ios::LocationSample sample;
    if (!location) return sample;
    sample.coordinate = {location.coordinate.latitude, location.coordinate.longitude};
    sample.altitude = location.altitude;
    sample.horizontalAccuracy = location.horizontalAccuracy;
    sample.verticalAccuracy = location.verticalAccuracy;
    sample.course = location.course;
    sample.speed = location.speed;
    sample.timestampSeconds = location.timestamp.timeIntervalSince1970;
    return sample;
}

CLLocationAccuracy TCXIOSNativeLocationAccuracy(tcx::ios::LocationAccuracy accuracy) {
    switch (accuracy) {
        case tcx::ios::LocationAccuracy::ThreeKilometers: return kCLLocationAccuracyThreeKilometers;
        case tcx::ios::LocationAccuracy::Kilometer: return kCLLocationAccuracyKilometer;
        case tcx::ios::LocationAccuracy::HundredMeters: return kCLLocationAccuracyHundredMeters;
        case tcx::ios::LocationAccuracy::NearestTenMeters: return kCLLocationAccuracyNearestTenMeters;
        case tcx::ios::LocationAccuracy::Best: return kCLLocationAccuracyBest;
        case tcx::ios::LocationAccuracy::BestForNavigation: return kCLLocationAccuracyBestForNavigation;
    }
    return kCLLocationAccuracyHundredMeters;
}

tcx::ios::NetworkPath TCXIOSMapNetworkPath(nw_path_t path) {
    tcx::ios::NetworkPath result;
    if (!path) return result;

    switch (nw_path_get_status(path)) {
        case nw_path_status_satisfied:
            result.status = tcx::ios::NetworkPathStatus::Satisfied;
            break;
        case nw_path_status_unsatisfied:
            result.status = tcx::ios::NetworkPathStatus::Unsatisfied;
            break;
        case nw_path_status_satisfiable:
            result.status = tcx::ios::NetworkPathStatus::RequiresConnection;
            break;
        default:
            result.status = tcx::ios::NetworkPathStatus::Unknown;
            break;
    }

    result.expensive = nw_path_is_expensive(path);
    if (@available(iOS 13.0, *)) {
        result.constrained = nw_path_is_constrained(path);
    }
    if (nw_path_uses_interface_type(path, nw_interface_type_wifi)) {
        result.interfaces.push_back(tcx::ios::NetworkInterface::WiFi);
    }
    if (nw_path_uses_interface_type(path, nw_interface_type_cellular)) {
        result.interfaces.push_back(tcx::ios::NetworkInterface::Cellular);
    }
    if (nw_path_uses_interface_type(path, nw_interface_type_wired)) {
        result.interfaces.push_back(tcx::ios::NetworkInterface::WiredEthernet);
    }
    if (nw_path_uses_interface_type(path, nw_interface_type_loopback)) {
        result.interfaces.push_back(tcx::ios::NetworkInterface::Loopback);
    }
    if (nw_path_uses_interface_type(path, nw_interface_type_other)) {
        result.interfaces.push_back(tcx::ios::NetworkInterface::Other);
    }
    return result;
}

} // namespace

@interface TCXIOSLocationDelegate : NSObject <CLLocationManagerDelegate>
@end

@implementation TCXIOSLocationDelegate

- (void)locationManagerDidChangeAuthorization:(CLLocationManager*)manager {
    tcx::ios::PermissionState state = TCXIOSMapCLAuthorization(manager.authorizationStatus);
    tcx::ios::Completion<tcx::ios::PermissionState> completion;
    {
        std::lock_guard<std::mutex> lock(gTCXIOSLocationMutex);
        if (state == tcx::ios::PermissionState::NotDetermined) return;
        completion = std::move(gTCXIOSLocationPermissionCompletion);
    }
    if (completion) {
        TCXIOSFinish(std::move(completion), tcx::ios::Result<tcx::ios::PermissionState>::success(state));
    }
}

- (void)locationManager:(CLLocationManager*)manager didChangeAuthorizationStatus:(CLAuthorizationStatus)status {
    [self locationManagerDidChangeAuthorization:manager];
}

- (void)locationManager:(CLLocationManager*)manager didUpdateLocations:(NSArray<CLLocation*>*)locations {
    CLLocation* location = locations.lastObject;
    if (!location) return;

    tcx::ios::LocationSample sample = TCXIOSMapLocation(location);
    tcx::ios::LocationHandler handler;
    {
        std::lock_guard<std::mutex> lock(gTCXIOSLocationMutex);
        gTCXIOSLatestLocation = sample;
        handler = gTCXIOSLocationHandler;
    }
    if (handler) {
        tcx::ios::eventQueue().post([handler, sample]() mutable {
            handler(tcx::ios::Result<tcx::ios::LocationSample>::success(sample));
        });
    }
}

- (void)locationManager:(CLLocationManager*)manager didFailWithError:(NSError*)error {
    tcx::ios::LocationHandler handler;
    {
        std::lock_guard<std::mutex> lock(gTCXIOSLocationMutex);
        handler = gTCXIOSLocationHandler;
    }
    if (handler) {
        tcx::ios::Error native = TCXIOSNativeError(error, "Location update failed.");
        tcx::ios::eventQueue().post([handler, native]() mutable {
            handler(tcx::ios::Result<tcx::ios::LocationSample>::failure(native));
        });
    }
}

@end

namespace {

CLLocationManager* TCXIOSLocationManagerInstance() {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        gTCXIOSLocationDelegate = [[TCXIOSLocationDelegate alloc] init];
        gTCXIOSLocationManager = [[CLLocationManager alloc] init];
        gTCXIOSLocationManager.delegate = gTCXIOSLocationDelegate;
    });
    return gTCXIOSLocationManager;
}

struct TCXIOSPendingDownload {
    std::string identifier;
    std::string sourceURL;
    std::filesystem::path destination;
    tcx::ios::Completion<tcx::ios::BackgroundDownloadResult> completion;
    tcx::ios::BackgroundDownloadProgressHandler progress;
};

} // namespace

@interface TCXIOSBackgroundDownloadCoordinator : NSObject <NSURLSessionDownloadDelegate>
+ (instancetype)shared;
- (void)startDownload:(const tcx::ios::BackgroundDownloadRequest&)request
           completion:(tcx::ios::Completion<tcx::ios::BackgroundDownloadResult>)completion
             progress:(tcx::ios::BackgroundDownloadProgressHandler)progress;
- (void)cancelDownload:(NSString*)identifier;
- (std::vector<tcx::ios::BackgroundDownloadRequest>)pendingRequests;
@end

@implementation TCXIOSBackgroundDownloadCoordinator {
    NSMutableDictionary<NSString*, NSURLSession*>* sessions_;
    std::mutex mutex_;
    std::map<NSUInteger, TCXIOSPendingDownload> pending_;
}

+ (instancetype)shared {
    static TCXIOSBackgroundDownloadCoordinator* coordinator = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        coordinator = [[TCXIOSBackgroundDownloadCoordinator alloc] init];
    });
    return coordinator;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        sessions_ = [NSMutableDictionary dictionary];
    }
    return self;
}

- (NSURLSession*)sessionForIdentifier:(NSString*)identifier allowsCellular:(BOOL)allowsCellular {
    NSURLSession* session = sessions_[identifier];
    if (session) return session;

    NSURLSessionConfiguration* configuration =
        [NSURLSessionConfiguration backgroundSessionConfigurationWithIdentifier:identifier];
    configuration.allowsCellularAccess = allowsCellular;
    configuration.sessionSendsLaunchEvents = YES;
    session = [NSURLSession sessionWithConfiguration:configuration delegate:self delegateQueue:nil];
    sessions_[identifier] = session;
    return session;
}

- (NSMutableDictionary*)downloadRegistry {
    NSUserDefaults* defaults = NSUserDefaults.standardUserDefaults;
    NSDictionary* stored = [defaults dictionaryForKey:@"tcxIOS.backgroundDownloads"];
    return stored ? [stored mutableCopy] : [NSMutableDictionary dictionary];
}

- (void)saveDownloadRegistry:(NSDictionary*)registry {
    [NSUserDefaults.standardUserDefaults setObject:registry forKey:@"tcxIOS.backgroundDownloads"];
}

- (void)storeRequest:(const tcx::ios::BackgroundDownloadRequest&)request
          identifier:(const std::string&)identifier
         destination:(const std::filesystem::path&)destination {
    if (!request.persistAcrossRelaunch) return;
    NSMutableDictionary* registry = [self downloadRegistry];
    registry[TCXIOSNs(identifier)] = @{
        @"url": TCXIOSNs(request.url),
        @"destination": TCXIOSNs(destination.string()),
        @"identifier": TCXIOSNs(identifier),
        @"sessionIdentifier": request.sessionIdentifier.empty()
            ? @"org.trussc.tcxios.background-downloads"
            : TCXIOSNs(request.sessionIdentifier),
        @"allowsCellularAccess": @(request.allowsCellularAccess),
        @"persistAcrossRelaunch": @(request.persistAcrossRelaunch)
    };
    [self saveDownloadRegistry:registry];
}

- (void)removeRequestWithIdentifier:(NSString*)identifier {
    NSMutableDictionary* registry = [self downloadRegistry];
    [registry removeObjectForKey:identifier];
    [self saveDownloadRegistry:registry];
}

- (std::vector<tcx::ios::BackgroundDownloadRequest>)pendingRequests {
    NSDictionary* registry = [self downloadRegistry];
    std::vector<tcx::ios::BackgroundDownloadRequest> requests;
    for (NSString* key in registry) {
        NSDictionary* item = registry[key];
        if (![item isKindOfClass:NSDictionary.class]) continue;
        tcx::ios::BackgroundDownloadRequest request;
        request.identifier = TCXIOSStr(item[@"identifier"] ?: key);
        request.url = TCXIOSStr(item[@"url"]);
        request.destination = std::filesystem::path(TCXIOSStr(item[@"destination"]));
        request.sessionIdentifier = TCXIOSStr(item[@"sessionIdentifier"]);
        NSNumber* allowsCellular = item[@"allowsCellularAccess"];
        if ([allowsCellular isKindOfClass:NSNumber.class]) {
            request.allowsCellularAccess = allowsCellular.boolValue;
        }
        request.persistAcrossRelaunch = true;
        requests.push_back(std::move(request));
    }
    return requests;
}

- (void)startDownload:(const tcx::ios::BackgroundDownloadRequest&)request
           completion:(tcx::ios::Completion<tcx::ios::BackgroundDownloadResult>)completion
             progress:(tcx::ios::BackgroundDownloadProgressHandler)progress {
    NSURL* url = [NSURL URLWithString:TCXIOSNs(request.url)];
    if (!url || url.scheme.length == 0) {
        TCXIOSFinish(std::move(completion),
                     tcx::ios::Result<tcx::ios::BackgroundDownloadResult>::failure({
                         tcx::ios::ErrorCode::InvalidArgument,
                         "BackgroundDownloadRequest has an invalid URL.",
                         0
                     }));
        return;
    }

    std::string identifier = request.identifier.empty() ? TCXIOSStr(NSUUID.UUID.UUIDString) : request.identifier;
    std::filesystem::path destination = request.destination;
    if (destination.empty()) {
        destination = std::filesystem::path(NSTemporaryDirectory().UTF8String) /
                      "tcxIOS-background-downloads" /
                      (identifier + ".download");
    }

    NSString* sessionIdentifier = request.sessionIdentifier.empty()
        ? @"org.trussc.tcxios.background-downloads"
        : TCXIOSNs(request.sessionIdentifier);
    NSURLSession* session = [self sessionForIdentifier:sessionIdentifier
                                        allowsCellular:request.allowsCellularAccess ? YES : NO];
    NSURLSessionDownloadTask* task = [session downloadTaskWithURL:url];
    task.taskDescription = TCXIOSNs(identifier);
    [self storeRequest:request identifier:identifier destination:destination];

    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_[task.taskIdentifier] = {
            identifier,
            request.url,
            destination,
            std::move(completion),
            std::move(progress)
        };
    }

    [task resume];
}

- (void)cancelDownload:(NSString*)identifier {
    NSArray<NSURLSession*>* sessions = sessions_.allValues;
    for (NSURLSession* session in sessions) {
        [session getAllTasksWithCompletionHandler:^(NSArray<__kindof NSURLSessionTask*>* tasks) {
            for (NSURLSessionTask* task in tasks) {
                if ([task.taskDescription isEqualToString:identifier]) {
                    [self removeRequestWithIdentifier:identifier];
                    [task cancel];
                }
            }
        }];
    }
}

- (void)URLSession:(NSURLSession*)session
      downloadTask:(NSURLSessionDownloadTask*)downloadTask
      didWriteData:(int64_t)bytesWritten
 totalBytesWritten:(int64_t)totalBytesWritten
totalBytesExpectedToWrite:(int64_t)totalBytesExpectedToWrite {
    tcx::ios::BackgroundDownloadProgressHandler handler;
    std::string identifier;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pending_.find(downloadTask.taskIdentifier);
        if (it == pending_.end()) return;
        handler = it->second.progress;
        identifier = it->second.identifier;
    }
    if (!handler) return;

    tcx::ios::BackgroundDownloadProgress progress;
    progress.identifier = identifier;
    progress.bytesWritten = bytesWritten;
    progress.totalBytesWritten = totalBytesWritten;
    progress.totalBytesExpected = totalBytesExpectedToWrite;
    if (totalBytesExpectedToWrite > 0) {
        progress.fractionCompleted = static_cast<double>(totalBytesWritten) /
                                     static_cast<double>(totalBytesExpectedToWrite);
    }
    tcx::ios::eventQueue().post([handler, progress]() mutable {
        handler(progress);
    });
}

- (void)URLSession:(NSURLSession*)session
      downloadTask:(NSURLSessionDownloadTask*)downloadTask
didFinishDownloadingToURL:(NSURL*)location {
    TCXIOSPendingDownload pending;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pending_.find(downloadTask.taskIdentifier);
        if (it == pending_.end()) return;
        pending = std::move(it->second);
        pending_.erase(it);
    }
    [self removeRequestWithIdentifier:TCXIOSNs(pending.identifier)];

    NSError* error = nil;
    NSFileManager* fileManager = NSFileManager.defaultManager;
    NSString* destinationPath = TCXIOSNs(pending.destination.string());
    NSString* directory = destinationPath.stringByDeletingLastPathComponent;
    if (![fileManager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:&error]) {
        TCXIOSFinish(std::move(pending.completion),
                     tcx::ios::Result<tcx::ios::BackgroundDownloadResult>::failure(
                         TCXIOSNativeError(error, "Failed to create download destination directory.")));
        return;
    }

    NSURL* destinationURL = [NSURL fileURLWithPath:destinationPath];
    [fileManager removeItemAtURL:destinationURL error:nil];
    if (![fileManager moveItemAtURL:location toURL:destinationURL error:&error]) {
        TCXIOSFinish(std::move(pending.completion),
                     tcx::ios::Result<tcx::ios::BackgroundDownloadResult>::failure(
                         TCXIOSNativeError(error, "Failed to move background download into place.")));
        return;
    }

    TCXIOSFinish(std::move(pending.completion),
                 tcx::ios::Result<tcx::ios::BackgroundDownloadResult>::success({
                     pending.identifier,
                     pending.destination,
                     pending.sourceURL
                 }));
}

- (void)URLSession:(NSURLSession*)session
              task:(NSURLSessionTask*)task
didCompleteWithError:(NSError*)error {
    if (!error) return;

    TCXIOSPendingDownload pending;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = pending_.find(task.taskIdentifier);
        if (it == pending_.end()) return;
        pending = std::move(it->second);
        pending_.erase(it);
    }
    [self removeRequestWithIdentifier:TCXIOSNs(pending.identifier)];

    TCXIOSFinish(std::move(pending.completion),
                 tcx::ios::Result<tcx::ios::BackgroundDownloadResult>::failure(
                     TCXIOSNativeError(error, "Background download failed.")));
}

@end


namespace {

std::mutex gTCXIOSNotificationMutex;
tcx::ios::NotificationResponseHandler gTCXIOSNotificationResponseHandler;

} // namespace

@interface TCXIOSNotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
+ (instancetype)shared;
@end

@implementation TCXIOSNotificationDelegate

+ (instancetype)shared {
    static TCXIOSNotificationDelegate* delegate = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        delegate = [[TCXIOSNotificationDelegate alloc] init];
    });
    return delegate;
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
       willPresentNotification:(UNNotification*)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions options))completionHandler {
    (void)center;
    (void)notification;
    completionHandler(UNNotificationPresentationOptionBanner |
                      UNNotificationPresentationOptionList |
                      UNNotificationPresentationOptionSound);
}

- (void)userNotificationCenter:(UNUserNotificationCenter*)center
didReceiveNotificationResponse:(UNNotificationResponse*)response
         withCompletionHandler:(void (^)(void))completionHandler {
    (void)center;
    tcx::ios::NotificationResponseHandler handler;
    {
        std::lock_guard<std::mutex> lock(gTCXIOSNotificationMutex);
        handler = gTCXIOSNotificationResponseHandler;
    }

    if (handler) {
        UNNotificationContent* content = response.notification.request.content;
        tcx::ios::NotificationResponse mapped;
        mapped.notificationIdentifier = TCXIOSStr(response.notification.request.identifier);
        mapped.actionIdentifier = TCXIOSStr(response.actionIdentifier);
        mapped.categoryIdentifier = TCXIOSStr(content.categoryIdentifier);
        mapped.title = TCXIOSStr(content.title);
        mapped.body = TCXIOSStr(content.body);
        tcx::ios::eventQueue().post([handler = std::move(handler), mapped = std::move(mapped)]() mutable {
            handler(mapped);
        });
    }
    completionHandler();
}

@end



namespace tcx::ios::detail {

namespace {

NSString* ns(const std::string& value) { return TCXIOSNs(value); }
std::string str(NSString* value) { return TCXIOSStr(value); }
Error nativeError(NSError* error, const std::string& fallback) { return TCXIOSNativeError(error, fallback); }

template <typename T>
void finish(Completion<T> done, Result<T> result) {
    TCXIOSFinish(std::move(done), std::move(result));
}

void finishVoid(Completion<void> done, Result<void> result) {
    TCXIOSFinishVoid(std::move(done), std::move(result));
}

UIViewController* presenter() { return TCXIOSPresenter(); }
UIWindow* activeWindow() { return TCXIOSActiveWindow(); }

} // namespace

PermissionState mapAVStatus(AVAuthorizationStatus status) {
    switch (status) {
        case AVAuthorizationStatusNotDetermined: return PermissionState::NotDetermined;
        case AVAuthorizationStatusRestricted: return PermissionState::Restricted;
        case AVAuthorizationStatusDenied: return PermissionState::Denied;
        case AVAuthorizationStatusAuthorized: return PermissionState::Authorized;
    }
    return PermissionState::Unknown;
}

PermissionState mapPhotoStatus(PHAuthorizationStatus status) {
    switch (status) {
        case PHAuthorizationStatusNotDetermined: return PermissionState::NotDetermined;
        case PHAuthorizationStatusRestricted: return PermissionState::Restricted;
        case PHAuthorizationStatusDenied: return PermissionState::Denied;
        case PHAuthorizationStatusAuthorized: return PermissionState::Authorized;
        case PHAuthorizationStatusLimited: return PermissionState::Limited;
    }
    return PermissionState::Unknown;
}

PermissionState mapMotionStatus(CMAuthorizationStatus status) {
    switch (status) {
        case CMAuthorizationStatusNotDetermined: return PermissionState::NotDetermined;
        case CMAuthorizationStatusRestricted: return PermissionState::Restricted;
        case CMAuthorizationStatusDenied: return PermissionState::Denied;
        case CMAuthorizationStatusAuthorized: return PermissionState::Authorized;
    }
    return PermissionState::Unknown;
}

PermissionState mapNotificationStatus(UNAuthorizationStatus status) {
    switch (status) {
        case UNAuthorizationStatusNotDetermined: return PermissionState::NotDetermined;
        case UNAuthorizationStatusDenied: return PermissionState::Denied;
        case UNAuthorizationStatusAuthorized: return PermissionState::Authorized;
        case UNAuthorizationStatusProvisional: return PermissionState::Provisional;
        case UNAuthorizationStatusEphemeral: return PermissionState::Provisional;
    }
    return PermissionState::Unknown;
}

NotificationSettingState mapNotificationSetting(UNNotificationSetting setting) {
    switch (setting) {
        case UNNotificationSettingNotSupported: return NotificationSettingState::NotSupported;
        case UNNotificationSettingDisabled: return NotificationSettingState::Disabled;
        case UNNotificationSettingEnabled: return NotificationSettingState::Enabled;
    }
    return NotificationSettingState::NotSupported;
}

std::atomic<int> gNotificationAuthorizationStatus{static_cast<int>(PermissionState::Unknown)};

PermissionState cachedNotificationAuthorizationStatus() {
    return static_cast<PermissionState>(gNotificationAuthorizationStatus.load());
}

void cacheNotificationAuthorizationStatus(PermissionState state) {
    gNotificationAuthorizationStatus.store(static_cast<int>(state));
}

NotificationSettings mapNotificationSettings(UNNotificationSettings* nativeSettings) {
    NotificationSettings settings;
    if (!nativeSettings) return settings;

    settings.authorizationStatus = mapNotificationStatus(nativeSettings.authorizationStatus);
    settings.alert = mapNotificationSetting(nativeSettings.alertSetting);
    settings.sound = mapNotificationSetting(nativeSettings.soundSetting);
    settings.badge = mapNotificationSetting(nativeSettings.badgeSetting);
    settings.notificationCenter = mapNotificationSetting(nativeSettings.notificationCenterSetting);
    settings.lockScreen = mapNotificationSetting(nativeSettings.lockScreenSetting);
    settings.carPlay = mapNotificationSetting(nativeSettings.carPlaySetting);
    settings.criticalAlert = mapNotificationSetting(nativeSettings.criticalAlertSetting);
    if (@available(iOS 15.0, *)) {
        settings.timeSensitive = mapNotificationSetting(nativeSettings.timeSensitiveSetting);
        settings.scheduledDeliveryEnabled = nativeSettings.scheduledDeliverySetting == UNNotificationSettingEnabled;
    }
    settings.providesAppNotificationSettings = nativeSettings.providesAppNotificationSettings;
    return settings;
}


PermissionState platformPermissionStatus(Permission permission) {
    switch (permission) {
        case Permission::Camera:
            return mapAVStatus([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo]);
        case Permission::Microphone: {
            AVAudioSessionRecordPermission status = [[AVAudioSession sharedInstance] recordPermission];
            switch (status) {
                case AVAudioSessionRecordPermissionUndetermined: return PermissionState::NotDetermined;
                case AVAudioSessionRecordPermissionDenied: return PermissionState::Denied;
                case AVAudioSessionRecordPermissionGranted: return PermissionState::Authorized;
            }
            return PermissionState::Unknown;
        }
        case Permission::PhotoLibraryRead:
            return mapPhotoStatus([PHPhotoLibrary authorizationStatusForAccessLevel:PHAccessLevelReadWrite]);
        case Permission::PhotoLibraryAddOnly:
            return mapPhotoStatus([PHPhotoLibrary authorizationStatusForAccessLevel:PHAccessLevelAddOnly]);
        case Permission::LocationWhenInUse:
        case Permission::LocationAlways:
            return TCXIOSMapCLAuthorization(TCXIOSLocationManagerInstance().authorizationStatus);
        case Permission::Notifications:
            return cachedNotificationAuthorizationStatus();
        case Permission::Bluetooth:
            return platformBluetoothPermissionStatus();
        case Permission::Motion:
            return platformMotionPermissionStatus();
        case Permission::Contacts:
            return platformContactsPermissionStatus();
        default:
            return PermissionState::Unknown;
    }
}

void platformRequestPermission(Permission permission, Completion<PermissionState> done) {
    if (permission == Permission::Camera) {
        if (!TCXIOSHasUsageDescription(permission, done)) return;
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted) {
            PermissionState state = granted ? PermissionState::Authorized : PermissionState::Denied;
            finish(std::move(done), Result<PermissionState>::success(state));
        }];
        return;
    }

    if (permission == Permission::Microphone) {
        if (!TCXIOSHasUsageDescription(permission, done)) return;
        [[AVAudioSession sharedInstance] requestRecordPermission:^(BOOL granted) {
            PermissionState state = granted ? PermissionState::Authorized : PermissionState::Denied;
            finish(std::move(done), Result<PermissionState>::success(state));
        }];
        return;
    }

    if (permission == Permission::PhotoLibraryRead) {
        if (!TCXIOSHasUsageDescription(permission, done)) return;
        [PHPhotoLibrary requestAuthorizationForAccessLevel:PHAccessLevelReadWrite handler:^(PHAuthorizationStatus status) {
            finish(std::move(done), Result<PermissionState>::success(mapPhotoStatus(status)));
        }];
        return;
    }

    if (permission == Permission::PhotoLibraryAddOnly) {
        if (!TCXIOSHasUsageDescription(permission, done)) return;
        [PHPhotoLibrary requestAuthorizationForAccessLevel:PHAccessLevelAddOnly handler:^(PHAuthorizationStatus status) {
            finish(std::move(done), Result<PermissionState>::success(mapPhotoStatus(status)));
        }];
        return;
    }

    if (permission == Permission::LocationWhenInUse) {
        platformRequestLocationWhenInUse(std::move(done));
        return;
    }

    if (permission == Permission::LocationAlways) {
        if (!TCXIOSHasUsageDescription(permission, done)) return;
        CLLocationManager* manager = TCXIOSLocationManagerInstance();
        {
            std::lock_guard<std::mutex> lock(gTCXIOSLocationMutex);
            gTCXIOSLocationPermissionCompletion = std::move(done);
        }
        dispatch_async(dispatch_get_main_queue(), ^{
            [manager requestAlwaysAuthorization];
        });
        return;
    }

    if (permission == Permission::Notifications) {
        UNAuthorizationOptions options = UNAuthorizationOptionAlert | UNAuthorizationOptionSound | UNAuthorizationOptionBadge;
        [[UNUserNotificationCenter currentNotificationCenter] requestAuthorizationWithOptions:options
                                                                            completionHandler:^(BOOL granted, NSError* error) {
            if (error) {
                finish(std::move(done), Result<PermissionState>::failure(nativeError(error, "Notification authorization failed.")));
            } else {
                PermissionState state = granted ? PermissionState::Authorized : PermissionState::Denied;
                cacheNotificationAuthorizationStatus(state);
                finish(std::move(done), Result<PermissionState>::success(state));
            }
        }];
        return;
    }

    if (permission == Permission::Contacts) {
        if (!TCXIOSHasUsageDescription(permission, done)) return;
        platformRequestContactsPermission(std::move(done));
        return;
    }

    finish(std::move(done), Result<PermissionState>::failure(notImplementedError("Permissions.request(" + toString(permission) + ")")));
}

PermissionState platformMotionPermissionStatus() {
    return mapMotionStatus([CMMotionActivityManager authorizationStatus]);
}


void platformGetNotificationSettings(Completion<NotificationSettings> done) {
    [[UNUserNotificationCenter currentNotificationCenter] getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings* nativeSettings) {
        NotificationSettings settings = mapNotificationSettings(nativeSettings);
        cacheNotificationAuthorizationStatus(settings.authorizationStatus);
        finish(std::move(done), Result<NotificationSettings>::success(std::move(settings)));
    }];
}

void platformSetNotificationCategories(const std::vector<NotificationCategory>& categories,
                                       Completion<void> done) {
    NSMutableSet<UNNotificationCategory*>* nativeCategories = [NSMutableSet set];
    for (const auto& category : categories) {
        NSMutableArray<UNNotificationAction*>* nativeActions = [NSMutableArray array];
        for (const auto& action : category.actions) {
            UNNotificationActionOptions options = 0;
            if (action.foreground) options |= UNNotificationActionOptionForeground;
            if (action.destructive) options |= UNNotificationActionOptionDestructive;
            UNNotificationAction* nativeAction =
                [UNNotificationAction actionWithIdentifier:ns(action.identifier)
                                                     title:ns(action.title)
                                                   options:options];
            [nativeActions addObject:nativeAction];
        }

        UNNotificationCategoryOptions options = 0;
        if (category.hiddenPreviewsShowTitle) {
            options |= UNNotificationCategoryOptionHiddenPreviewsShowTitle;
        }
        UNNotificationCategory* nativeCategory =
            [UNNotificationCategory categoryWithIdentifier:ns(category.identifier)
                                                   actions:nativeActions
                                         intentIdentifiers:@[]
                                                   options:options];
        [nativeCategories addObject:nativeCategory];
    }
    UNUserNotificationCenter.currentNotificationCenter.delegate = [TCXIOSNotificationDelegate shared];
    [UNUserNotificationCenter.currentNotificationCenter setNotificationCategories:nativeCategories];
    finishVoid(std::move(done), Result<void>::success());
}

void platformScheduleNotification(const LocalNotificationRequest& request, Completion<std::string> done) {
    std::string identifier = request.identifier.empty() ? "tcxios-local-notification" : request.identifier;

    UNMutableNotificationContent* content = [[UNMutableNotificationContent alloc] init];
    content.title = ns(request.title);
    content.body = ns(request.body);
    content.categoryIdentifier = ns(request.categoryIdentifier);
    content.sound = [UNNotificationSound defaultSound];

    NSTimeInterval delay = request.delaySeconds < 1.0 ? 1.0 : request.delaySeconds;
    UNTimeIntervalNotificationTrigger* trigger = [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:delay
                                                                                                    repeats:request.repeats];
    UNNotificationRequest* nativeRequest = [UNNotificationRequest requestWithIdentifier:ns(identifier)
                                                                                content:content
                                                                                trigger:trigger];
    [[UNUserNotificationCenter currentNotificationCenter] addNotificationRequest:nativeRequest
                                                           withCompletionHandler:^(NSError* error) {
        if (error) {
            finish(std::move(done), Result<std::string>::failure(nativeError(error, "Notification scheduling failed.")));
        } else {
            finish(std::move(done), Result<std::string>::success(identifier));
        }
    }];
}

void platformCancelNotification(const std::string& identifier) {
    NSArray<NSString*>* identifiers = @[ns(identifier)];
    [[UNUserNotificationCenter currentNotificationCenter] removePendingNotificationRequestsWithIdentifiers:identifiers];
    [[UNUserNotificationCenter currentNotificationCenter] removeDeliveredNotificationsWithIdentifiers:identifiers];
}

void platformCancelAllNotifications() {
    [[UNUserNotificationCenter currentNotificationCenter] removeAllPendingNotificationRequests];
    [[UNUserNotificationCenter currentNotificationCenter] removeAllDeliveredNotifications];
}

void platformSetNotificationResponseHandler(NotificationResponseHandler handler) {
    {
        std::lock_guard<std::mutex> lock(gTCXIOSNotificationMutex);
        gTCXIOSNotificationResponseHandler = std::move(handler);
    }
    UNUserNotificationCenter.currentNotificationCenter.delegate = [TCXIOSNotificationDelegate shared];
}

void platformClearNotificationResponseHandler() {
    std::lock_guard<std::mutex> lock(gTCXIOSNotificationMutex);
    gTCXIOSNotificationResponseHandler = nullptr;
}


PermissionState platformLocationAuthorizationStatus() {
    return TCXIOSMapCLAuthorization(TCXIOSLocationManagerInstance().authorizationStatus);
}

void platformRequestLocationWhenInUse(Completion<PermissionState> done) {
    if (!TCXIOSHasUsageDescription(Permission::LocationWhenInUse, done)) return;
    CLLocationManager* manager = TCXIOSLocationManagerInstance();
    PermissionState current = TCXIOSMapCLAuthorization(manager.authorizationStatus);
    if (current != PermissionState::NotDetermined && current != PermissionState::Unknown) {
        finish(std::move(done), Result<PermissionState>::success(current));
        return;
    }

    {
        std::lock_guard<std::mutex> lock(gTCXIOSLocationMutex);
        gTCXIOSLocationPermissionCompletion = std::move(done);
    }
    dispatch_async(dispatch_get_main_queue(), ^{
        [manager requestWhenInUseAuthorization];
    });
}

void platformStartLocation(const LocationConfig& config, LocationHandler handler) {
    CLLocationManager* manager = TCXIOSLocationManagerInstance();
    PermissionState state = TCXIOSMapCLAuthorization(manager.authorizationStatus);
    if (state == PermissionState::Denied) {
        if (handler) {
            eventQueue().post([handler = std::move(handler)]() mutable {
                handler(Result<LocationSample>::failure({ErrorCode::PermissionDenied, "Location permission was denied.", 0}));
            });
        }
        return;
    }
    if (state == PermissionState::Restricted) {
        if (handler) {
            eventQueue().post([handler = std::move(handler)]() mutable {
                handler(Result<LocationSample>::failure({ErrorCode::PermissionRestricted, "Location permission is restricted.", 0}));
            });
        }
        return;
    }
    if (state == PermissionState::NotDetermined || state == PermissionState::Unknown) {
        if (handler) {
            eventQueue().post([handler = std::move(handler)]() mutable {
                handler(Result<LocationSample>::failure({ErrorCode::InvalidState, "Request location permission before starting updates.", 0}));
            });
        }
        return;
    }
    if (!CLLocationManager.locationServicesEnabled) {
        if (handler) {
            eventQueue().post([handler = std::move(handler)]() mutable {
                handler(Result<LocationSample>::failure({ErrorCode::Unavailable, "Location services are disabled.", 0}));
            });
        }
        return;
    }

    {
        std::lock_guard<std::mutex> lock(gTCXIOSLocationMutex);
        gTCXIOSLocationHandler = std::move(handler);
    }
    dispatch_async(dispatch_get_main_queue(), ^{
        manager.desiredAccuracy = TCXIOSNativeLocationAccuracy(config.accuracy);
        manager.distanceFilter = config.distanceFilterMeters < 0.0
            ? kCLDistanceFilterNone
            : config.distanceFilterMeters;
        [manager startUpdatingLocation];
        gTCXIOSLocationRunning.store(true);
    });
}

void platformStopLocation() {
    CLLocationManager* manager = TCXIOSLocationManagerInstance();
    dispatch_async(dispatch_get_main_queue(), ^{
        [manager stopUpdatingLocation];
        gTCXIOSLocationRunning.store(false);
        std::lock_guard<std::mutex> lock(gTCXIOSLocationMutex);
        gTCXIOSLocationHandler = nullptr;
    });
}

bool platformLocationIsRunning() {
    return gTCXIOSLocationRunning.load();
}

bool platformLatestLocation(LocationSample& out) {
    std::lock_guard<std::mutex> lock(gTCXIOSLocationMutex);
    if (gTCXIOSLatestLocation.timestampSeconds <= 0.0) return false;
    out = gTCXIOSLatestLocation;
    return true;
}

NetworkPath platformCurrentNetworkPath() {
    std::lock_guard<std::mutex> lock(gTCXIOSNetworkMutex);
    return gTCXIOSLatestNetworkPath;
}

void platformStartNetworkStatus(NetworkPathHandler handler) {
    {
        std::lock_guard<std::mutex> lock(gTCXIOSNetworkMutex);
        gTCXIOSNetworkHandler = std::move(handler);
    }

    if (gTCXIOSNetworkMonitor) return;

    gTCXIOSNetworkMonitor = nw_path_monitor_create();
    nw_path_monitor_set_queue(gTCXIOSNetworkMonitor, TCXIOSNetworkQueue());
    nw_path_monitor_set_update_handler(gTCXIOSNetworkMonitor, ^(nw_path_t path) {
        NetworkPath mapped = TCXIOSMapNetworkPath(path);
        NetworkPathHandler currentHandler;
        {
            std::lock_guard<std::mutex> lock(gTCXIOSNetworkMutex);
            gTCXIOSLatestNetworkPath = mapped;
            currentHandler = gTCXIOSNetworkHandler;
        }
        if (currentHandler) {
            eventQueue().post([currentHandler, mapped]() mutable {
                currentHandler(mapped);
            });
        }
    });
    nw_path_monitor_start(gTCXIOSNetworkMonitor);
    gTCXIOSNetworkRunning.store(true);
}

void platformStopNetworkStatus() {
    if (!gTCXIOSNetworkMonitor) return;
    nw_path_monitor_cancel(gTCXIOSNetworkMonitor);
    gTCXIOSNetworkMonitor = nil;
    gTCXIOSNetworkRunning.store(false);
    std::lock_guard<std::mutex> lock(gTCXIOSNetworkMutex);
    gTCXIOSNetworkHandler = nullptr;
}

bool platformNetworkStatusIsRunning() {
    return gTCXIOSNetworkRunning.load();
}

void platformRegisterBackgroundTask(const BackgroundTaskRegistration& registration,
                                    BackgroundTaskHandler handler,
                                    Completion<void> done) {
    if (registration.identifier.empty()) {
        finishVoid(std::move(done), Result<void>::failure({ErrorCode::InvalidArgument, "Background task identifier is empty.", 0}));
        return;
    }

    if (@available(iOS 13.0, *)) {
        const std::string identifier = registration.identifier;
        const BackgroundTaskKind kind = registration.kind;
        {
            std::lock_guard<std::mutex> lock(gTCXIOSBackgroundTaskMutex);
            gTCXIOSBackgroundTaskHandlers[identifier] = std::move(handler);
            gTCXIOSBackgroundTaskKinds[identifier] = kind;
        }

        BOOL ok = [BGTaskScheduler.sharedScheduler registerForTaskWithIdentifier:ns(identifier)
                                                                       usingQueue:nil
                                                                    launchHandler:^(BGTask* task) {
            __block BOOL completed = NO;
            task.expirationHandler = ^{
                if (completed) return;
                completed = YES;
                BackgroundTaskHandler expirationHandler;
                BackgroundTaskKind expirationKind = kind;
                {
                    std::lock_guard<std::mutex> lock(gTCXIOSBackgroundTaskMutex);
                    auto handlerIt = gTCXIOSBackgroundTaskHandlers.find(identifier);
                    if (handlerIt != gTCXIOSBackgroundTaskHandlers.end()) expirationHandler = handlerIt->second;
                    auto kindIt = gTCXIOSBackgroundTaskKinds.find(identifier);
                    if (kindIt != gTCXIOSBackgroundTaskKinds.end()) expirationKind = kindIt->second;
                }
                if (expirationHandler) {
                    expirationHandler({identifier, expirationKind, true, "BGTask expirationHandler"});
                }
                [task setTaskCompletedWithSuccess:NO];
            };

            BackgroundTaskHandler currentHandler;
            BackgroundTaskKind currentKind = kind;
            {
                std::lock_guard<std::mutex> lock(gTCXIOSBackgroundTaskMutex);
                auto handlerIt = gTCXIOSBackgroundTaskHandlers.find(identifier);
                if (handlerIt != gTCXIOSBackgroundTaskHandlers.end()) currentHandler = handlerIt->second;
                auto kindIt = gTCXIOSBackgroundTaskKinds.find(identifier);
                if (kindIt != gTCXIOSBackgroundTaskKinds.end()) currentKind = kindIt->second;
            }

            BOOL success = YES;
            if (currentHandler) {
                success = currentHandler({identifier, currentKind, false, {}}) ? YES : NO;
            }
            if (!completed) {
                completed = YES;
                [task setTaskCompletedWithSuccess:success];
            }
        }];

        finishVoid(std::move(done), ok
            ? Result<void>::success()
            : Result<void>::failure({ErrorCode::NativeError, "BGTaskScheduler rejected registration. Check permitted identifiers in Info.plist.", 0}));
        return;
    }

    finishVoid(std::move(done), Result<void>::failure({ErrorCode::Unavailable, "BackgroundTasks requires iOS 13 or newer.", 0}));
}

void platformScheduleBackgroundTask(const BackgroundTaskRequest& request, Completion<void> done) {
    if (request.identifier.empty()) {
        finishVoid(std::move(done), Result<void>::failure({ErrorCode::InvalidArgument, "Background task identifier is empty.", 0}));
        return;
    }

    if (@available(iOS 13.0, *)) {
        BGTaskRequest* nativeRequest = nil;
        if (request.kind == BackgroundTaskKind::Processing) {
            BGProcessingTaskRequest* processing =
                [[BGProcessingTaskRequest alloc] initWithIdentifier:ns(request.identifier)];
            processing.requiresNetworkConnectivity = request.requiresNetworkConnectivity;
            processing.requiresExternalPower = request.requiresExternalPower;
            nativeRequest = processing;
        } else {
            nativeRequest = [[BGAppRefreshTaskRequest alloc] initWithIdentifier:ns(request.identifier)];
        }
        if (request.earliestBeginSecondsFromNow > 0.0) {
            nativeRequest.earliestBeginDate =
                [NSDate dateWithTimeIntervalSinceNow:request.earliestBeginSecondsFromNow];
        }

        NSError* error = nil;
        BOOL ok = [BGTaskScheduler.sharedScheduler submitTaskRequest:nativeRequest error:&error];
        if (ok) {
            finishVoid(std::move(done), Result<void>::success());
        } else {
            finishVoid(std::move(done), Result<void>::failure(nativeError(error, "Background task scheduling failed.")));
        }
        return;
    }

    finishVoid(std::move(done), Result<void>::failure({ErrorCode::Unavailable, "BackgroundTasks requires iOS 13 or newer.", 0}));
}

void platformCancelBackgroundTask(const std::string& identifier) {
    if (@available(iOS 13.0, *)) {
        [BGTaskScheduler.sharedScheduler cancelTaskRequestWithIdentifier:ns(identifier)];
    }
}

void platformCancelAllBackgroundTasks() {
    if (@available(iOS 13.0, *)) {
        [BGTaskScheduler.sharedScheduler cancelAllTaskRequests];
    }
}

void platformStartBackgroundDownload(const BackgroundDownloadRequest& request,
                                     Completion<BackgroundDownloadResult> done,
                                     BackgroundDownloadProgressHandler progress) {
    [[TCXIOSBackgroundDownloadCoordinator shared] startDownload:request
                                                    completion:std::move(done)
                                                      progress:std::move(progress)];
}

void platformCancelBackgroundDownload(const std::string& identifier) {
    [[TCXIOSBackgroundDownloadCoordinator shared] cancelDownload:ns(identifier)];
}

std::vector<BackgroundDownloadRequest> platformPendingBackgroundDownloads() {
    return [[TCXIOSBackgroundDownloadCoordinator shared] pendingRequests];
}


} // namespace tcx::ios::detail
