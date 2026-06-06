#include "TCXIOSPlatform.h"
#include "tcx/ios/EventQueue.h"

#import <ARKit/ARKit.h>
#import <Contacts/Contacts.h>
#import <ContactsUI/ContactsUI.h>
#import <CoreBluetooth/CoreBluetooth.h>
#import <CoreImage/CoreImage.h>
#import <CoreML/CoreML.h>
#import <GameController/GameController.h>
#import <MultipeerConnectivity/MultipeerConnectivity.h>
#import <PencilKit/PencilKit.h>
#import <StoreKit/StoreKit.h>
#import <UIKit/UIKit.h>
#import <Vision/Vision.h>

#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <utility>

namespace {

NSString* TCXIOSV03Ns(const std::string& value) {
    return [NSString stringWithUTF8String:value.c_str()];
}

std::string TCXIOSV03Str(NSString* value) {
    return value ? std::string(value.UTF8String) : std::string();
}

tcx::ios::Error TCXIOSV03NativeError(NSError* error, const std::string& fallback) {
    tcx::ios::Error mapped = !error ? tcx::ios::Error{tcx::ios::ErrorCode::NativeError, fallback, 0}
        : tcx::ios::Error{
        tcx::ios::ErrorCode::NativeError,
        TCXIOSV03Str(error.localizedDescription),
        static_cast<int>(error.code)
    };
    tcx::ios::logger().error("tcxIOS.native", fallback, mapped);
    return mapped;
}

template <typename T>
void TCXIOSV03Finish(tcx::ios::Completion<T> done, tcx::ios::Result<T> result) {
    if (!done) return;
    tcx::ios::eventQueue().post([done = std::move(done), result = std::move(result)]() mutable {
        done(std::move(result));
    });
}

void TCXIOSV03FinishVoid(tcx::ios::Completion<void> done, tcx::ios::Result<void> result) {
    if (!done) return;
    tcx::ios::eventQueue().post([done = std::move(done), result = std::move(result)]() mutable {
        done(std::move(result));
    });
}

NSMutableSet* TCXIOSV03Delegates() {
    static NSMutableSet* delegates = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        delegates = [NSMutableSet set];
    });
    return delegates;
}

void TCXIOSV03Retain(id delegate) {
    @synchronized (TCXIOSV03Delegates()) {
        [TCXIOSV03Delegates() addObject:delegate];
    }
}

void TCXIOSV03Release(id delegate) {
    @synchronized (TCXIOSV03Delegates()) {
        [TCXIOSV03Delegates() removeObject:delegate];
    }
}

UIViewController* TCXIOSV03TopViewController(UIViewController* root) {
    UIViewController* current = root;
    while (current.presentedViewController) current = current.presentedViewController;
    if ([current isKindOfClass:UINavigationController.class]) {
        UIViewController* visible = ((UINavigationController*)current).visibleViewController;
        if (visible) return TCXIOSV03TopViewController(visible);
    }
    if ([current isKindOfClass:UITabBarController.class]) {
        UIViewController* selected = ((UITabBarController*)current).selectedViewController;
        if (selected) return TCXIOSV03TopViewController(selected);
    }
    return current;
}

UIViewController* TCXIOSV03Presenter() {
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if (![scene isKindOfClass:UIWindowScene.class]) continue;
        UIWindowScene* windowScene = (UIWindowScene*)scene;
        if (windowScene.activationState != UISceneActivationStateForegroundActive) continue;
        for (UIWindow* window in windowScene.windows) {
            if (window.isKeyWindow && window.rootViewController) {
                return TCXIOSV03TopViewController(window.rootViewController);
            }
        }
    }
    return nil;
}

NSArray<CBUUID*>* TCXIOSV03CBUUIDs(const std::vector<std::string>& uuids) {
    NSMutableArray<CBUUID*>* out = [NSMutableArray array];
    for (const auto& uuid : uuids) {
        [out addObject:[CBUUID UUIDWithString:TCXIOSV03Ns(uuid)]];
    }
    return out.count > 0 ? out : nil;
}

std::string TCXIOSV03PeripheralIdentifier(CBPeripheral* peripheral) {
    return TCXIOSV03Str(peripheral.identifier.UUIDString);
}

std::string TCXIOSV03CharacteristicKey(const tcx::ios::BLECharacteristicRef& ref) {
    return ref.peripheralIdentifier + "|" + ref.serviceUUID + "|" + ref.characteristicUUID;
}

std::string TCXIOSV03CharacteristicKey(CBPeripheral* peripheral, CBCharacteristic* characteristic) {
    return TCXIOSV03PeripheralIdentifier(peripheral) + "|" +
           TCXIOSV03Str(characteristic.service.UUID.UUIDString) + "|" +
           TCXIOSV03Str(characteristic.UUID.UUIDString);
}

tcx::ios::BluetoothState TCXIOSV03BluetoothState(CBManagerState state) {
    switch (state) {
        case CBManagerStateUnsupported: return tcx::ios::BluetoothState::Unsupported;
        case CBManagerStateUnauthorized: return tcx::ios::BluetoothState::Unauthorized;
        case CBManagerStatePoweredOff: return tcx::ios::BluetoothState::PoweredOff;
        case CBManagerStatePoweredOn: return tcx::ios::BluetoothState::PoweredOn;
        case CBManagerStateResetting:
        case CBManagerStateUnknown: return tcx::ios::BluetoothState::Unknown;
    }
    return tcx::ios::BluetoothState::Unknown;
}

tcx::ios::PermissionState TCXIOSV03BluetoothPermissionState(tcx::ios::BluetoothState state) {
    switch (state) {
        case tcx::ios::BluetoothState::Unsupported:
            return tcx::ios::PermissionState::Restricted;
        case tcx::ios::BluetoothState::Unauthorized:
            return tcx::ios::PermissionState::Denied;
        case tcx::ios::BluetoothState::PoweredOff:
        case tcx::ios::BluetoothState::PoweredOn:
            return tcx::ios::PermissionState::Authorized;
        case tcx::ios::BluetoothState::Unknown:
            return tcx::ios::PermissionState::Unknown;
    }
    return tcx::ios::PermissionState::Unknown;
}

tcx::ios::PermissionState TCXIOSV03ContactPermissionState(CNAuthorizationStatus status) {
    switch (status) {
        case CNAuthorizationStatusNotDetermined:
            return tcx::ios::PermissionState::NotDetermined;
        case CNAuthorizationStatusRestricted:
            return tcx::ios::PermissionState::Restricted;
        case CNAuthorizationStatusDenied:
            return tcx::ios::PermissionState::Denied;
        case CNAuthorizationStatusAuthorized:
            return tcx::ios::PermissionState::Authorized;
        case CNAuthorizationStatusLimited:
            return tcx::ios::PermissionState::Limited;
    }
    return tcx::ios::PermissionState::Unknown;
}

std::vector<std::uint8_t> TCXIOSV03Bytes(NSData* data) {
    std::vector<std::uint8_t> bytes(data.length);
    if (data.length > 0) std::memcpy(bytes.data(), data.bytes, data.length);
    return bytes;
}

NSData* TCXIOSV03Data(const std::vector<std::uint8_t>& bytes) {
    return [NSData dataWithBytes:bytes.data() length:bytes.size()];
}

} // namespace

@interface TCXIOSContactPickerDelegate : NSObject <CNContactPickerDelegate>
- (instancetype)initWithCompletion:(tcx::ios::Completion<tcx::ios::PickedContact>)completion;
@end

@implementation TCXIOSContactPickerDelegate {
    tcx::ios::Completion<tcx::ios::PickedContact> completion_;
}

- (instancetype)initWithCompletion:(tcx::ios::Completion<tcx::ios::PickedContact>)completion {
    self = [super init];
    if (self) completion_ = std::move(completion);
    return self;
}

- (void)contactPickerDidCancel:(CNContactPickerViewController*)picker {
    TCXIOSV03Release(self);
    TCXIOSV03Finish(std::move(completion_), tcx::ios::Result<tcx::ios::PickedContact>::failure({
        tcx::ios::ErrorCode::Cancelled,
        "Contact picker was cancelled.",
        0
    }));
}

- (void)contactPicker:(CNContactPickerViewController*)picker didSelectContact:(CNContact*)contact {
    tcx::ios::PickedContact picked;
    picked.identifier = TCXIOSV03Str(contact.identifier);
    picked.givenName = TCXIOSV03Str(contact.givenName);
    picked.familyName = TCXIOSV03Str(contact.familyName);
    for (CNLabeledValue<CNPhoneNumber*>* item in contact.phoneNumbers) {
        picked.phoneNumbers.push_back(TCXIOSV03Str(item.value.stringValue));
    }
    for (CNLabeledValue<NSString*>* item in contact.emailAddresses) {
        picked.emailAddresses.push_back(TCXIOSV03Str(item.value));
    }
    TCXIOSV03Release(self);
    TCXIOSV03Finish(std::move(completion_), tcx::ios::Result<tcx::ios::PickedContact>::success(std::move(picked)));
}

@end

namespace {

ARSession* gARSession = nil;
id gARDelegate = nil;
std::mutex gTCXIOSV03ARMutex;
tcx::ios::ARFrameInfo gTCXIOSV03LatestARFrame;

} // namespace

namespace tcx::ios::detail {
void platformPickContact(Completion<PickedContact> done) {
    dispatch_async(dispatch_get_main_queue(), ^{
        UIViewController* presenter = TCXIOSV03Presenter();
        if (!presenter) {
            TCXIOSV03Finish(std::move(done), Result<PickedContact>::failure({ErrorCode::InvalidState, "No active presenter for ContactsUI.", 0}));
            return;
        }
        CNContactPickerViewController* picker = [[CNContactPickerViewController alloc] init];
        TCXIOSContactPickerDelegate* delegate = [[TCXIOSContactPickerDelegate alloc] initWithCompletion:std::move(done)];
        picker.delegate = delegate;
        TCXIOSV03Retain(delegate);
        [presenter presentViewController:picker animated:YES completion:nil];
    });
}

PermissionState platformContactsPermissionStatus() {
    return TCXIOSV03ContactPermissionState([CNContactStore authorizationStatusForEntityType:CNEntityTypeContacts]);
}

void platformRequestContactsPermission(Completion<PermissionState> done) {
    CNContactStore* store = [[CNContactStore alloc] init];
    [store requestAccessForEntityType:CNEntityTypeContacts completionHandler:^(BOOL granted, NSError* error) {
        if (error) {
            TCXIOSV03Finish(std::move(done), Result<PermissionState>::failure(TCXIOSV03NativeError(error, "Contacts authorization failed.")));
            return;
        }
        TCXIOSV03Finish(std::move(done), Result<PermissionState>::success(granted ? PermissionState::Authorized : PermissionState::Denied));
    }];
}

} // namespace tcx::ios::detail
