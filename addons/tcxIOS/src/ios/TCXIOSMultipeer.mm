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

@interface TCXIOSMultipeerCoordinator : NSObject <MCSessionDelegate, MCNearbyServiceAdvertiserDelegate, MCNearbyServiceBrowserDelegate>
+ (instancetype)shared;
- (void)start:(const tcx::ios::MultipeerConfig&)config completion:(tcx::ios::Completion<void>)completion;
- (void)stop;
- (std::vector<tcx::ios::MultipeerPeer>)peers;
- (void)send:(const std::vector<std::uint8_t>&)data completion:(tcx::ios::Completion<void>)completion;
- (void)setPeerHandler:(tcx::ios::MultipeerPeerHandler)handler;
- (void)setMessageHandler:(tcx::ios::MultipeerMessageHandler)handler;
@end

@implementation TCXIOSMultipeerCoordinator {
    MCPeerID* peerID_;
    MCSession* session_;
    MCNearbyServiceAdvertiser* advertiser_;
    MCNearbyServiceBrowser* browser_;
    BOOL autoInvite_;
    BOOL autoAccept_;
    std::mutex mutex_;
    tcx::ios::MultipeerPeerHandler peerHandler_;
    tcx::ios::MultipeerMessageHandler messageHandler_;
}

+ (instancetype)shared {
    static TCXIOSMultipeerCoordinator* coordinator = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        coordinator = [[TCXIOSMultipeerCoordinator alloc] init];
    });
    return coordinator;
}

- (void)start:(const tcx::ios::MultipeerConfig&)config completion:(tcx::ios::Completion<void>)completion {
    NSString* serviceType = config.serviceType.empty() ? @"tcxios" : TCXIOSV03Ns(config.serviceType);
    NSString* displayName = config.displayName.empty()
        ? UIDevice.currentDevice.name
        : TCXIOSV03Ns(config.displayName);
    peerID_ = [[MCPeerID alloc] initWithDisplayName:displayName];
    session_ = [[MCSession alloc] initWithPeer:peerID_ securityIdentity:nil encryptionPreference:MCEncryptionRequired];
    session_.delegate = self;
    autoInvite_ = config.autoInvite ? YES : NO;
    autoAccept_ = config.autoAccept ? YES : NO;
    advertiser_ = [[MCNearbyServiceAdvertiser alloc] initWithPeer:peerID_ discoveryInfo:nil serviceType:serviceType];
    browser_ = [[MCNearbyServiceBrowser alloc] initWithPeer:peerID_ serviceType:serviceType];
    advertiser_.delegate = self;
    browser_.delegate = self;
    [advertiser_ startAdvertisingPeer];
    [browser_ startBrowsingForPeers];
    TCXIOSV03FinishVoid(std::move(completion), tcx::ios::Result<void>::success());
}

- (void)stop {
    [advertiser_ stopAdvertisingPeer];
    [browser_ stopBrowsingForPeers];
    [session_ disconnect];
    advertiser_ = nil;
    browser_ = nil;
    session_ = nil;
    peerID_ = nil;
}

- (std::vector<tcx::ios::MultipeerPeer>)peers {
    std::vector<tcx::ios::MultipeerPeer> out;
    for (MCPeerID* peer in session_.connectedPeers) {
        out.push_back({TCXIOSV03Str(peer.displayName), TCXIOSV03Str(peer.displayName), true});
    }
    return out;
}

- (void)send:(const std::vector<std::uint8_t>&)data completion:(tcx::ios::Completion<void>)completion {
    if (!session_ || session_.connectedPeers.count == 0) {
        TCXIOSV03FinishVoid(std::move(completion), tcx::ios::Result<void>::failure({
            tcx::ios::ErrorCode::InvalidState,
            "No connected multipeer peers.",
            0
        }));
        return;
    }
    NSError* error = nil;
    BOOL ok = [session_ sendData:TCXIOSV03Data(data)
                         toPeers:session_.connectedPeers
                        withMode:MCSessionSendDataReliable
                           error:&error];
    TCXIOSV03FinishVoid(std::move(completion), ok
        ? tcx::ios::Result<void>::success()
        : tcx::ios::Result<void>::failure(TCXIOSV03NativeError(error, "Multipeer send failed.")));
}

- (void)setPeerHandler:(tcx::ios::MultipeerPeerHandler)handler {
    std::lock_guard<std::mutex> lock(mutex_);
    peerHandler_ = std::move(handler);
}

- (void)setMessageHandler:(tcx::ios::MultipeerMessageHandler)handler {
    std::lock_guard<std::mutex> lock(mutex_);
    messageHandler_ = std::move(handler);
}

- (void)dispatchPeers {
    tcx::ios::MultipeerPeerHandler handler;
    std::vector<tcx::ios::MultipeerPeer> peers = [self peers];
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handler = peerHandler_;
    }
    if (handler) {
        tcx::ios::eventQueue().post([handler, peers]() mutable {
            handler(peers);
        });
    }
}

- (void)session:(MCSession*)session peer:(MCPeerID*)peerID didChangeState:(MCSessionState)state {
    [self dispatchPeers];
}

- (void)session:(MCSession*)session didReceiveData:(NSData*)data fromPeer:(MCPeerID*)peerID {
    tcx::ios::MultipeerMessageHandler handler;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        handler = messageHandler_;
    }
    if (!handler) return;
    tcx::ios::MultipeerMessage message;
    message.peer = {TCXIOSV03Str(peerID.displayName), TCXIOSV03Str(peerID.displayName), true};
    message.data = TCXIOSV03Bytes(data);
    tcx::ios::eventQueue().post([handler, message]() mutable {
        handler(message);
    });
}

- (void)browser:(MCNearbyServiceBrowser*)browser foundPeer:(MCPeerID*)peerID withDiscoveryInfo:(NSDictionary<NSString*, NSString*>*)info {
    if (autoInvite_) [browser invitePeer:peerID toSession:session_ withContext:nil timeout:10.0];
}

- (void)advertiser:(MCNearbyServiceAdvertiser*)advertiser
didReceiveInvitationFromPeer:(MCPeerID*)peerID
       withContext:(NSData*)context
 invitationHandler:(void (^)(BOOL accept, MCSession* session))invitationHandler {
    invitationHandler(autoAccept_, session_);
}

- (void)session:(MCSession*)session didReceiveStream:(NSInputStream*)stream withName:(NSString*)streamName fromPeer:(MCPeerID*)peerID {}
- (void)session:(MCSession*)session didStartReceivingResourceWithName:(NSString*)resourceName fromPeer:(MCPeerID*)peerID withProgress:(NSProgress*)progress {}
- (void)session:(MCSession*)session didFinishReceivingResourceWithName:(NSString*)resourceName fromPeer:(MCPeerID*)peerID atURL:(NSURL*)localURL withError:(NSError*)error {}
- (void)browser:(MCNearbyServiceBrowser*)browser lostPeer:(MCPeerID*)peerID { [self dispatchPeers]; }
- (void)browser:(MCNearbyServiceBrowser*)browser didNotStartBrowsingForPeers:(NSError*)error {}
- (void)advertiser:(MCNearbyServiceAdvertiser*)advertiser didNotStartAdvertisingPeer:(NSError*)error {}

@end

namespace tcx::ios::detail {
void platformStartMultipeer(const MultipeerConfig& config, Completion<void> done) {
    [[TCXIOSMultipeerCoordinator shared] start:config completion:std::move(done)];
}

void platformStopMultipeer() {
    [[TCXIOSMultipeerCoordinator shared] stop];
}

std::vector<MultipeerPeer> platformMultipeerPeers() {
    return [[TCXIOSMultipeerCoordinator shared] peers];
}

void platformMultipeerSend(const std::vector<std::uint8_t>& data, Completion<void> done) {
    [[TCXIOSMultipeerCoordinator shared] send:data completion:std::move(done)];
}

void platformSetMultipeerPeerHandler(MultipeerPeerHandler handler) {
    [[TCXIOSMultipeerCoordinator shared] setPeerHandler:std::move(handler)];
}

void platformSetMultipeerMessageHandler(MultipeerMessageHandler handler) {
    [[TCXIOSMultipeerCoordinator shared] setMessageHandler:std::move(handler)];
}

} // namespace tcx::ios::detail
