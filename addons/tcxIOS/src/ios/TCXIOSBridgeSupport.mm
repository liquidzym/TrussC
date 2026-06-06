#include "TCXIOSBridgeSupport.h"

namespace {

NSMutableSet* TCXIOSActiveDelegates() {
    static NSMutableSet* delegates = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        delegates = [NSMutableSet set];
    });
    return delegates;
}

NSMapTable<NSString*, UIViewController*>* TCXIOSScenePresenters() {
    static NSMapTable<NSString*, UIViewController*>* presenters = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        presenters = [NSMapTable strongToWeakObjectsMapTable];
    });
    return presenters;
}

NSString* TCXIOSInfoPlistKeyForPermission(tcx::ios::Permission permission) {
    switch (permission) {
        case tcx::ios::Permission::Camera: return @"NSCameraUsageDescription";
        case tcx::ios::Permission::Microphone: return @"NSMicrophoneUsageDescription";
        case tcx::ios::Permission::PhotoLibraryRead: return @"NSPhotoLibraryUsageDescription";
        case tcx::ios::Permission::PhotoLibraryAddOnly: return @"NSPhotoLibraryAddUsageDescription";
        case tcx::ios::Permission::Motion: return @"NSMotionUsageDescription";
        case tcx::ios::Permission::LocationWhenInUse: return @"NSLocationWhenInUseUsageDescription";
        case tcx::ios::Permission::LocationAlways: return @"NSLocationAlwaysAndWhenInUseUsageDescription";
        case tcx::ios::Permission::Bluetooth: return @"NSBluetoothAlwaysUsageDescription";
        case tcx::ios::Permission::Contacts: return @"NSContactsUsageDescription";
        case tcx::ios::Permission::Notifications: return nil;
    }
    return nil;
}

} // namespace

NSString* TCXIOSNs(const std::string& value) {
    return [NSString stringWithUTF8String:value.c_str()];
}

std::string TCXIOSStr(NSString* value) {
    return value ? std::string([value UTF8String]) : std::string();
}

tcx::ios::Error TCXIOSNativeError(NSError* error, const std::string& fallback) {
    tcx::ios::Error mapped = !error ? tcx::ios::Error{tcx::ios::ErrorCode::NativeError, fallback, 0}
        : tcx::ios::Error{
        tcx::ios::ErrorCode::NativeError,
        TCXIOSStr(error.localizedDescription),
        static_cast<int>(error.code)
    };
    tcx::ios::logger().error("tcxIOS.native", fallback, mapped);
    return mapped;
}

namespace tcx::ios::detail {

void TCXIOSFinishVoid(tcx::ios::Completion<void> done, tcx::ios::Result<void> result) {
    if (!done) return;
    tcx::ios::eventQueue().post([done = std::move(done), result = std::move(result)]() mutable {
        done(std::move(result));
    });
}

} // namespace tcx::ios::detail

void TCXIOSRetainDelegate(id delegate) {
    @synchronized (TCXIOSActiveDelegates()) {
        [TCXIOSActiveDelegates() addObject:delegate];
    }
}

void TCXIOSReleaseDelegate(id delegate) {
    @synchronized (TCXIOSActiveDelegates()) {
        [TCXIOSActiveDelegates() removeObject:delegate];
    }
}

void TCXIOSSetScenePresenter(NSString* identifier, UIViewController* viewController) {
    if (identifier.length == 0) return;
    NSMapTable<NSString*, UIViewController*>* presenters = TCXIOSScenePresenters();
    @synchronized (presenters) {
        if (viewController) {
            [presenters setObject:viewController forKey:identifier];
        } else {
            [presenters removeObjectForKey:identifier];
        }
    }
}

UIViewController* TCXIOSScenePresenter(NSString* identifier) {
    if (identifier.length == 0) return nil;
    NSMapTable<NSString*, UIViewController*>* presenters = TCXIOSScenePresenters();
    @synchronized (presenters) {
        return [presenters objectForKey:identifier];
    }
}

void TCXIOSRemoveScenePresenter(NSString* identifier) {
    if (identifier.length == 0) return;
    NSMapTable<NSString*, UIViewController*>* presenters = TCXIOSScenePresenters();
    @synchronized (presenters) {
        [presenters removeObjectForKey:identifier];
    }
}

UIViewController* TCXIOSTopViewController(UIViewController* root) {
    UIViewController* current = root;
    while (current.presentedViewController) {
        current = current.presentedViewController;
    }
    if ([current isKindOfClass:[UINavigationController class]]) {
        UIViewController* visible = [(UINavigationController*)current visibleViewController];
        if (visible) return TCXIOSTopViewController(visible);
    }
    if ([current isKindOfClass:[UITabBarController class]]) {
        UIViewController* selected = [(UITabBarController*)current selectedViewController];
        if (selected) return TCXIOSTopViewController(selected);
    }
    return current;
}

UIWindow* TCXIOSActiveWindow() {
    NSSet<UIScene*>* scenes = [[UIApplication sharedApplication] connectedScenes];
    for (UIScene* scene in scenes) {
        if (![scene isKindOfClass:[UIWindowScene class]]) continue;
        if (scene.activationState != UISceneActivationStateForegroundActive) continue;
        UIWindowScene* windowScene = (UIWindowScene*)scene;
        for (UIWindow* window in windowScene.windows) {
            if (window.isKeyWindow) return window;
        }
        if (windowScene.windows.count > 0) return windowScene.windows.firstObject;
    }

    for (UIScene* scene in scenes) {
        if (![scene isKindOfClass:[UIWindowScene class]]) continue;
        UIWindowScene* windowScene = (UIWindowScene*)scene;
        if (windowScene.windows.count > 0) return windowScene.windows.firstObject;
    }

    return nil;
}

UIViewController* TCXIOSPresenter() {
    std::string activeIdentifier = tcx::ios::scene().activeIdentifier();
    if (!activeIdentifier.empty()) {
        UIViewController* registered = TCXIOSScenePresenter(TCXIOSNs(activeIdentifier));
        if (registered) return TCXIOSTopViewController(registered);
    }

    UIWindow* window = TCXIOSActiveWindow();
    if (!window.rootViewController) return nil;
    return TCXIOSTopViewController(window.rootViewController);
}

NSURL* TCXIOSTemporaryCopyURL(NSURL* sourceURL, NSString* directoryName, NSError** error) {
    NSString* filename = sourceURL.lastPathComponent.length > 0
        ? sourceURL.lastPathComponent
        : [[NSUUID UUID].UUIDString stringByAppendingPathExtension:@"dat"];
    NSString* directory = [NSTemporaryDirectory() stringByAppendingPathComponent:directoryName];
    NSFileManager* fileManager = [NSFileManager defaultManager];
    if (![fileManager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:error]) {
        return nil;
    }

    NSString* uniqueName = [NSString stringWithFormat:@"%@-%@", [NSUUID UUID].UUIDString, filename];
    NSString* destinationPath = [directory stringByAppendingPathComponent:uniqueName];
    NSURL* destinationURL = [NSURL fileURLWithPath:destinationPath];
    [fileManager removeItemAtURL:destinationURL error:nil];
    if (![fileManager copyItemAtURL:sourceURL toURL:destinationURL error:error]) {
        return nil;
    }
    return destinationURL;
}

bool TCXIOSHasUsageDescription(tcx::ios::Permission permission, tcx::ios::Completion<tcx::ios::PermissionState>& done) {
    NSString* key = TCXIOSInfoPlistKeyForPermission(permission);
    if (!key) return true;
    id value = [[NSBundle mainBundle] objectForInfoDictionaryKey:key];
    if ([value isKindOfClass:[NSString class]] && [value length] > 0) return true;
    TCXIOSFinish(std::move(done), tcx::ios::Result<tcx::ios::PermissionState>::failure({
        tcx::ios::ErrorCode::InvalidState,
        "Missing Info.plist key before permission request: " + TCXIOSStr(key),
        0
    }));
    return false;
}

NSArray<UTType*>* TCXIOSContentTypes(const std::vector<std::string>& identifiers) {
    NSMutableArray<UTType*>* types = [NSMutableArray array];
    for (const auto& identifier : identifiers) {
        UTType* type = [UTType typeWithIdentifier:TCXIOSNs(identifier)];
        if (!type) type = [UTType typeWithFilenameExtension:TCXIOSNs(identifier)];
        if (type) [types addObject:type];
    }
    if (types.count == 0) [types addObject:UTTypeItem];
    return types;
}
