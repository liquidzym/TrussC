#include "TCXIOSBridgeSupport.h"

#import <UIKit/UIKit.h>

#include <vector>

@interface TCXIOSAppObserver : NSObject
+ (instancetype)shared;
- (void)syncScenes;
@end

@implementation TCXIOSAppObserver

+ (instancetype)shared {
    static TCXIOSAppObserver* observer = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        observer = [[TCXIOSAppObserver alloc] init];
    });
    return observer;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
        [center addObserver:self selector:@selector(appStateChanged:)
                       name:UIApplicationDidBecomeActiveNotification object:nil];
        [center addObserver:self selector:@selector(appStateChanged:)
                       name:UIApplicationWillResignActiveNotification object:nil];
        [center addObserver:self selector:@selector(appStateChanged:)
                       name:UIApplicationDidEnterBackgroundNotification object:nil];
        [center addObserver:self selector:@selector(appStateChanged:)
                       name:UIApplicationWillEnterForegroundNotification object:nil];
        [center addObserver:self selector:@selector(orientationChanged:)
                       name:UIDeviceOrientationDidChangeNotification object:nil];
        [center addObserver:self selector:@selector(screenChanged:)
                       name:UIScreenDidConnectNotification object:nil];
        [center addObserver:self selector:@selector(screenChanged:)
                       name:UIScreenDidDisconnectNotification object:nil];
        [[UIDevice currentDevice] beginGeneratingDeviceOrientationNotifications];
    }
    return self;
}

- (void)appStateChanged:(NSNotification*)notification {
    (void)notification;
    tcx::ios::AppState state = tcx::ios::detail::platformAppState();
    [self syncScenes];
    tcx::ios::eventQueue().post([state]() {
        if (tcx::ios::app().onStateChanged) tcx::ios::app().onStateChanged(state);
    });
}

- (void)orientationChanged:(NSNotification*)notification {
    (void)notification;
    tcx::ios::Orientation orientation = tcx::ios::detail::platformOrientation();
    tcx::ios::SafeAreaInsets safeArea = tcx::ios::detail::platformSafeAreaInsets();
    [self syncScenes];
    tcx::ios::eventQueue().post([orientation, safeArea]() {
        if (tcx::ios::app().onOrientationChanged) tcx::ios::app().onOrientationChanged(orientation);
        if (tcx::ios::app().onSafeAreaChanged) tcx::ios::app().onSafeAreaChanged(safeArea);
    });
}

- (void)screenChanged:(NSNotification*)notification {
    (void)notification;
    tcx::ios::ScreenInfo screen = tcx::ios::detail::platformMainScreen();
    [self syncScenes];
    tcx::ios::eventQueue().post([screen]() {
        if (tcx::ios::app().onScreenChanged) tcx::ios::app().onScreenChanged(screen);
    });
}

- (void)syncScenes {
    for (UIScene* nativeScene in UIApplication.sharedApplication.connectedScenes) {
        if (![nativeScene isKindOfClass:UIWindowScene.class]) continue;
        UIWindowScene* windowScene = (UIWindowScene*)nativeScene;
        tcx::ios::SceneContext context;
        context.identifier = TCXIOSStr(windowScene.session.persistentIdentifier);
        context.active = windowScene.activationState == UISceneActivationStateForegroundActive;
        UIWindow* window = windowScene.keyWindow;
        if (!window && windowScene.windows.count > 0) window = windowScene.windows.firstObject;
        if (window) {
            UIEdgeInsets insets = window.safeAreaInsets;
            context.safeArea = {
                static_cast<float>(insets.top),
                static_cast<float>(insets.left),
                static_cast<float>(insets.bottom),
                static_cast<float>(insets.right)
            };
        }
        tcx::ios::scene().upsertContext(std::move(context));
    }
}

@end

@interface TCXIOSBridge : NSObject
+ (void)registerSceneWithIdentifier:(NSString*)identifier
                     viewController:(UIViewController*)viewController;
+ (void)unregisterSceneWithIdentifier:(NSString*)identifier;
+ (void)setActiveSceneIdentifier:(NSString*)identifier;
@end

@implementation TCXIOSBridge

+ (void)registerSceneWithIdentifier:(NSString*)identifier
                     viewController:(UIViewController*)viewController {
    TCXIOSSetScenePresenter(identifier, viewController);
    tcx::ios::SceneContext context;
    context.identifier = TCXIOSStr(identifier);
    context.active = true;
    if (viewController && viewController.view) {
        UIEdgeInsets insets = viewController.view.safeAreaInsets;
        context.safeArea = {
            static_cast<float>(insets.top),
            static_cast<float>(insets.left),
            static_cast<float>(insets.bottom),
            static_cast<float>(insets.right)
        };
    }
    tcx::ios::scene().setActiveContext(std::move(context));
}

+ (void)unregisterSceneWithIdentifier:(NSString*)identifier {
    TCXIOSRemoveScenePresenter(identifier);
    tcx::ios::scene().removeContext(TCXIOSStr(identifier));
}

+ (void)setActiveSceneIdentifier:(NSString*)identifier {
    tcx::ios::scene().setActiveIdentifier(TCXIOSStr(identifier));
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

AppState platformAppState() {
    [TCXIOSAppObserver shared];
    UIApplicationState state = [[UIApplication sharedApplication] applicationState];
    switch (state) {
        case UIApplicationStateActive: return AppState::Active;
        case UIApplicationStateInactive: return AppState::Inactive;
        case UIApplicationStateBackground: return AppState::Background;
    }
    return AppState::Inactive;
}

ScreenInfo platformMainScreen() {
    [TCXIOSAppObserver shared];
    UIScreen* screen = [UIScreen mainScreen];
    CGSize size = screen.bounds.size;
    CGFloat scale = screen.scale;
    return {
        static_cast<int>(size.width * scale),
        static_cast<int>(size.height * scale),
        static_cast<float>(scale),
        static_cast<int>(screen.maximumFramesPerSecond)
    };
}

SafeAreaInsets platformSafeAreaInsets() {
    [[TCXIOSAppObserver shared] syncScenes];
    UIWindow* window = activeWindow();
    if (!window) return {};
    UIEdgeInsets insets = window.safeAreaInsets;
    return {
        static_cast<float>(insets.top),
        static_cast<float>(insets.left),
        static_cast<float>(insets.bottom),
        static_cast<float>(insets.right)
    };
}

Orientation platformOrientation() {
    [TCXIOSAppObserver shared];
    UIDeviceOrientation orientation = [[UIDevice currentDevice] orientation];
    switch (orientation) {
        case UIDeviceOrientationPortrait: return Orientation::Portrait;
        case UIDeviceOrientationPortraitUpsideDown: return Orientation::PortraitUpsideDown;
        case UIDeviceOrientationLandscapeLeft: return Orientation::LandscapeLeft;
        case UIDeviceOrientationLandscapeRight: return Orientation::LandscapeRight;
        default: return Orientation::Unknown;
    }
}

DeviceInfo platformDeviceInfo() {
    UIDevice* device = [UIDevice currentDevice];
    NSString* language = [[NSLocale preferredLanguages] firstObject];
    return {
        str(device.model),
        str(device.systemName),
        str(device.systemVersion),
        str(language),
        [[NSProcessInfo processInfo] isLowPowerModeEnabled]
    };
}

void platformShowAlert(const AlertRequest& request, Completion<AlertResult> done) {
    dispatch_async(dispatch_get_main_queue(), ^{
        UIViewController* viewController = presenter();
        if (!viewController) {
            finish(std::move(done), Result<AlertResult>::failure({ErrorCode::InvalidState, "No active view controller for alert.", 0}));
            return;
        }

        UIAlertController* alert = [UIAlertController alertControllerWithTitle:ns(request.title)
                                                                       message:ns(request.message)
                                                                preferredStyle:UIAlertControllerStyleAlert];

        std::vector<std::string> buttons = request.buttons.empty()
            ? std::vector<std::string>{"OK"}
            : request.buttons;

        for (std::size_t i = 0; i < buttons.size(); ++i) {
            UIAlertActionStyle style = (static_cast<int>(i) == request.cancelButtonIndex)
                ? UIAlertActionStyleCancel
                : UIAlertActionStyleDefault;
            int index = static_cast<int>(i);
            UIAlertAction* action = [UIAlertAction actionWithTitle:ns(buttons[i])
                                                             style:style
                                                           handler:^(__unused UIAlertAction* action) {
                finish(std::move(done), Result<AlertResult>::success({index}));
            }];
            [alert addAction:action];
        }

        [viewController presentViewController:alert animated:YES completion:nil];
    });
}

void platformShare(const ShareRequest& request, Completion<ShareResult> done) {
    dispatch_async(dispatch_get_main_queue(), ^{
        UIViewController* viewController = presenter();
        if (!viewController) {
            finish(std::move(done), Result<ShareResult>::failure({ErrorCode::InvalidState, "No active view controller for share sheet.", 0}));
            return;
        }

        NSMutableArray* items = [NSMutableArray array];
        for (const auto& text : request.texts) {
            [items addObject:ns(text)];
        }
        for (const auto& file : request.files) {
            [items addObject:[NSURL fileURLWithPath:ns(file.string())]];
        }
        if (items.count == 0) {
            finish(std::move(done), Result<ShareResult>::failure({ErrorCode::InvalidArgument, "ShareRequest has no files or texts.", 0}));
            return;
        }

        UIActivityViewController* activity = [[UIActivityViewController alloc] initWithActivityItems:items applicationActivities:nil];
        if (!request.subject.empty()) {
            [activity setValue:ns(request.subject) forKey:@"subject"];
        }
        NSMutableArray<UIActivityType>* excluded = [NSMutableArray array];
        if (request.excludeAirDrop) [excluded addObject:UIActivityTypeAirDrop];
        if (request.excludePrint) [excluded addObject:UIActivityTypePrint];
        activity.excludedActivityTypes = excluded;

        activity.completionWithItemsHandler = ^(__unused UIActivityType activityType,
                                                BOOL completed,
                                                __unused NSArray* returnedItems,
                                                NSError* error) {
            if (error) {
                finish(std::move(done), Result<ShareResult>::failure(nativeError(error, "Share failed.")));
            } else {
                finish(std::move(done), Result<ShareResult>::success({completed}));
            }
        };

        UIPopoverPresentationController* popover = activity.popoverPresentationController;
        if (popover) {
            popover.sourceView = viewController.view;
            popover.sourceRect = viewController.view.bounds;
        }

        [viewController presentViewController:activity animated:YES completion:nil];
    });
}

void platformOpenSettings() {
    platformOpenURL(str(UIApplicationOpenSettingsURLString), nullptr);
}

void platformOpenURL(const std::string& url, Completion<void> done) {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSURL* nativeURL = [NSURL URLWithString:ns(url)];
        if (!nativeURL) {
            finishVoid(std::move(done), Result<void>::failure({ErrorCode::InvalidArgument, "Invalid URL.", 0}));
            return;
        }
        [[UIApplication sharedApplication] openURL:nativeURL
                                           options:@{}
                                 completionHandler:^(BOOL success) {
            if (success) {
                finishVoid(std::move(done), Result<void>::success());
            } else {
                finishVoid(std::move(done), Result<void>::failure({ErrorCode::NativeError, "UIApplication failed to open URL.", 0}));
            }
        }];
    });
}


} // namespace tcx::ios::detail
