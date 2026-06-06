#include "TCXIOSBridgeSupport.h"

#import <LocalAuthentication/LocalAuthentication.h>
#import <SafariServices/SafariServices.h>
#import <Security/Security.h>
#import <UIKit/UIKit.h>

#include <cstring>
#include <vector>


namespace {

LAPolicy TCXIOSNativeAuthPolicy(tcx::ios::AuthenticationPolicy policy) {
    switch (policy) {
        case tcx::ios::AuthenticationPolicy::DeviceOwnerAuthentication:
            return LAPolicyDeviceOwnerAuthentication;
        case tcx::ios::AuthenticationPolicy::DeviceOwnerAuthenticationWithBiometrics:
            return LAPolicyDeviceOwnerAuthenticationWithBiometrics;
    }
    return LAPolicyDeviceOwnerAuthentication;
}

std::string TCXIOSBiometryTypeString(LABiometryType type) {
    switch (type) {
        case LABiometryTypeNone: return "none";
        case LABiometryTypeTouchID: return "touch id";
        case LABiometryTypeFaceID: return "face id";
        case LABiometryTypeOpticID: return "optic id";
    }
    return "unknown";
}

} // namespace

@interface TCXIOSSafariDelegate : NSObject <SFSafariViewControllerDelegate>
- (instancetype)initWithCompletion:(tcx::ios::Completion<void>)completion;
@end

@implementation TCXIOSSafariDelegate {
    tcx::ios::Completion<void> completion_;
}

- (instancetype)initWithCompletion:(tcx::ios::Completion<void>)completion {
    self = [super init];
    if (self) completion_ = std::move(completion);
    return self;
}

- (void)safariViewControllerDidFinish:(SFSafariViewController*)controller {
    [controller dismissViewControllerAnimated:YES completion:nil];
    TCXIOSReleaseDelegate(self);
    TCXIOSFinishVoid(std::move(completion_), tcx::ios::Result<void>::success());
}

@end


@interface TCXIOSExternalDisplayObserver : NSObject
@end

@implementation TCXIOSExternalDisplayObserver

- (instancetype)init {
    self = [super init];
    if (self) {
        NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
        [center addObserver:self selector:@selector(screenChanged:)
                       name:UIScreenDidConnectNotification object:nil];
        [center addObserver:self selector:@selector(screenChanged:)
                       name:UIScreenDidDisconnectNotification object:nil];
    }
    return self;
}

- (void)screenChanged:(NSNotification*)notification {
    tcx::ios::detail::dispatchExternalDisplaysChanged(tcx::ios::detail::platformExternalScreens());
}

@end

namespace {

TCXIOSExternalDisplayObserver* TCXIOSExternalDisplayObserverInstance() {
    static TCXIOSExternalDisplayObserver* observer = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        observer = [[TCXIOSExternalDisplayObserver alloc] init];
    });
    return observer;
}

NSMutableDictionary<NSString*, UIWindow*>* TCXIOSExternalDisplayWindows() {
    static NSMutableDictionary<NSString*, UIWindow*>* windows = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        windows = [NSMutableDictionary dictionary];
    });
    return windows;
}

} // namespace



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

NSMutableDictionary* keychainQuery(const std::string& service, const std::string& account) {
    return [@{
        (__bridge id)kSecClass: (__bridge id)kSecClassGenericPassword,
        (__bridge id)kSecAttrService: ns(service),
        (__bridge id)kSecAttrAccount: ns(account)
    } mutableCopy];
}

CFStringRef keychainAccessibility(KeychainAccessibility accessibility) {
    switch (accessibility) {
        case KeychainAccessibility::WhenUnlocked: return kSecAttrAccessibleWhenUnlocked;
        case KeychainAccessibility::AfterFirstUnlock: return kSecAttrAccessibleAfterFirstUnlock;
    }
    return kSecAttrAccessibleWhenUnlocked;
}

Error keychainStatusError(OSStatus status, const std::string& fallback) {
    CFStringRef message = SecCopyErrorMessageString(status, nullptr);
    std::string text = fallback;
    if (message) {
        text = str((__bridge NSString*)message);
        CFRelease(message);
    }
    return {ErrorCode::NativeError, text, static_cast<int>(status)};
}


Result<void> platformKeychainSet(const KeychainItem& item) {
    if (item.service.empty() || item.account.empty()) {
        return Result<void>::failure({ErrorCode::InvalidArgument, "Keychain service and account are required.", 0});
    }

    NSMutableDictionary* query = keychainQuery(item.service, item.account);
    SecItemDelete((__bridge CFDictionaryRef)query);

    NSData* data = [NSData dataWithBytes:item.data.data() length:item.data.size()];
    query[(__bridge id)kSecValueData] = data;
    query[(__bridge id)kSecAttrAccessible] = (__bridge id)keychainAccessibility(item.accessibility);

    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)query, nullptr);
    if (status == errSecSuccess) return Result<void>::success();
    return Result<void>::failure(keychainStatusError(status, "Keychain set failed."));
}

Result<std::vector<std::uint8_t>> platformKeychainGet(const std::string& service,
                                                      const std::string& account) {
    if (service.empty() || account.empty()) {
        return Result<std::vector<std::uint8_t>>::failure({ErrorCode::InvalidArgument, "Keychain service and account are required.", 0});
    }

    NSMutableDictionary* query = keychainQuery(service, account);
    query[(__bridge id)kSecReturnData] = @YES;
    query[(__bridge id)kSecMatchLimit] = (__bridge id)kSecMatchLimitOne;

    CFTypeRef item = nullptr;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &item);
    if (status != errSecSuccess) {
        if (item) CFRelease(item);
        Error error = status == errSecItemNotFound
            ? Error{ErrorCode::InvalidState, "Keychain item was not found.", static_cast<int>(status)}
            : keychainStatusError(status, "Keychain get failed.");
        return Result<std::vector<std::uint8_t>>::failure(error);
    }

    NSData* data = (NSData*)item;
    std::vector<std::uint8_t> bytes(data.length);
    if (data.length > 0) {
        std::memcpy(bytes.data(), data.bytes, data.length);
    }
    CFRelease(item);
    return Result<std::vector<std::uint8_t>>::success(std::move(bytes));
}

Result<void> platformKeychainRemove(const std::string& service, const std::string& account) {
    if (service.empty() || account.empty()) {
        return Result<void>::failure({ErrorCode::InvalidArgument, "Keychain service and account are required.", 0});
    }

    NSMutableDictionary* query = keychainQuery(service, account);
    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
    if (status == errSecSuccess || status == errSecItemNotFound) return Result<void>::success();
    return Result<void>::failure(keychainStatusError(status, "Keychain remove failed."));
}

AuthenticationAvailability platformAuthenticationAvailability(AuthenticationPolicy policy) {
    LAContext* context = [[LAContext alloc] init];
    NSError* error = nil;
    BOOL available = [context canEvaluatePolicy:TCXIOSNativeAuthPolicy(policy) error:&error];
    AuthenticationAvailability result;
    result.available = available == YES;
    result.biometryType = TCXIOSBiometryTypeString(context.biometryType);
    if (!available) result.error = nativeError(error, "Authentication policy is unavailable.");
    return result;
}

void platformEvaluateAuthentication(const AuthenticationRequest& request,
                                    Completion<AuthenticationResult> done) {
    LAContext* context = [[LAContext alloc] init];
    NSError* error = nil;
    LAPolicy policy = TCXIOSNativeAuthPolicy(request.policy);
    if (![context canEvaluatePolicy:policy error:&error]) {
        finish(std::move(done), Result<AuthenticationResult>::failure(nativeError(error, "Authentication policy is unavailable.")));
        return;
    }

    [context evaluatePolicy:policy
            localizedReason:ns(request.reason)
                      reply:^(BOOL success, NSError* authError) {
        if (success) {
            finish(std::move(done), Result<AuthenticationResult>::success({
                true,
                TCXIOSBiometryTypeString(context.biometryType)
            }));
        } else {
            finish(std::move(done), Result<AuthenticationResult>::failure(nativeError(authError, "Authentication failed.")));
        }
    }];
}


void platformOpenSafari(const SafariRequest& request, Completion<void> done) {
    dispatch_async(dispatch_get_main_queue(), ^{
        UIViewController* viewController = presenter();
        if (!viewController) {
            finishVoid(std::move(done), Result<void>::failure({ErrorCode::InvalidState, "No active view controller for Safari.", 0}));
            return;
        }

        NSURL* url = [NSURL URLWithString:ns(request.url)];
        if (!url || url.scheme.length == 0) {
            finishVoid(std::move(done), Result<void>::failure({ErrorCode::InvalidArgument, "SafariRequest has an invalid URL.", 0}));
            return;
        }

        SFSafariViewControllerConfiguration* configuration = [[SFSafariViewControllerConfiguration alloc] init];
        configuration.entersReaderIfAvailable = request.entersReaderIfAvailable;
        configuration.barCollapsingEnabled = request.barCollapsingEnabled;
        SFSafariViewController* safari = [[SFSafariViewController alloc] initWithURL:url configuration:configuration];
        TCXIOSSafariDelegate* delegate = [[TCXIOSSafariDelegate alloc] initWithCompletion:std::move(done)];
        safari.delegate = delegate;
        TCXIOSRetainDelegate(delegate);
        [viewController presentViewController:safari animated:YES completion:nil];
    });
}

std::vector<ExternalScreenInfo> platformExternalScreens() {
    std::vector<ExternalScreenInfo> screens;
    NSArray<UIScreen*>* nativeScreens = UIScreen.screens;
    for (NSUInteger i = 0; i < nativeScreens.count; ++i) {
        UIScreen* screen = nativeScreens[i];
        CGSize size = screen.bounds.size;
        CGFloat scale = screen.scale;
        screens.push_back({
            "screen-" + std::to_string(i),
            static_cast<int>(size.width * scale),
            static_cast<int>(size.height * scale),
            static_cast<float>(scale),
            static_cast<int>(screen.maximumFramesPerSecond)
        });
    }
    return screens;
}

void platformStartExternalDisplayObserving() {
    TCXIOSExternalDisplayObserverInstance();
}

void platformStopExternalDisplayObserving() {}

void platformShowExternalDisplay(const ExternalDisplayRequest& request,
                                 Completion<ExternalDisplayPresentation> done) {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSArray<UIScreen*>* screens = UIScreen.screens;
        if (screens.count <= 1) {
            finish(std::move(done), Result<ExternalDisplayPresentation>::failure({
                ErrorCode::Unavailable,
                "No external display is connected.",
                0
            }));
            return;
        }

        NSUInteger selectedIndex = 1;
        if (!request.screenIdentifier.empty() && request.screenIdentifier.rfind("screen-", 0) == 0) {
            std::string number = request.screenIdentifier.substr(7);
            try {
                selectedIndex = static_cast<NSUInteger>(std::stoul(number));
            } catch (...) {
                selectedIndex = 1;
            }
        }
        if (selectedIndex == 0 || selectedIndex >= screens.count) {
            finish(std::move(done), Result<ExternalDisplayPresentation>::failure({
                ErrorCode::InvalidArgument,
                "External display screen identifier is invalid.",
                0
            }));
            return;
        }

        UIScreen* screen = screens[selectedIndex];
        NSString* identifier = [NSString stringWithFormat:@"screen-%lu", (unsigned long)selectedIndex];
        UIWindow* window = [[UIWindow alloc] initWithFrame:screen.bounds];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        window.screen = screen;
#pragma clang diagnostic pop
        UIViewController* controller = [[UIViewController alloc] init];
        controller.view.backgroundColor = UIColor.blackColor;
        UILabel* label = [[UILabel alloc] initWithFrame:controller.view.bounds];
        label.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
        label.textAlignment = NSTextAlignmentCenter;
        label.textColor = UIColor.whiteColor;
        label.numberOfLines = 2;
        label.text = request.title.empty() ? @"tcxIOS External Display" : ns(request.title);
        [controller.view addSubview:label];
        window.rootViewController = controller;
        window.hidden = NO;
        [TCXIOSExternalDisplayWindows() setObject:window forKey:identifier];

        finish(std::move(done), Result<ExternalDisplayPresentation>::success({
            TCXIOSStr(identifier),
            true
        }));
    });
}

void platformDismissExternalDisplay(const std::string& screenIdentifier) {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString* identifier = ns(screenIdentifier);
        UIWindow* window = TCXIOSExternalDisplayWindows()[identifier];
        if (window) {
            window.hidden = YES;
            [TCXIOSExternalDisplayWindows() removeObjectForKey:identifier];
        }
    });
}

void platformDismissAllExternalDisplays() {
    dispatch_async(dispatch_get_main_queue(), ^{
        NSMutableDictionary<NSString*, UIWindow*>* windows = TCXIOSExternalDisplayWindows();
        for (UIWindow* window in windows.allValues) {
            window.hidden = YES;
        }
        [windows removeAllObjects];
    });
}

} // namespace tcx::ios::detail
