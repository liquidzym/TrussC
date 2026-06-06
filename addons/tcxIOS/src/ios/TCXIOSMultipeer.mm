#include "TCXIOSBridgeSupport.h"

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
    NSString* serviceType = config.serviceType.empty() ? @"tcxios" : TCXIOSNs(config.serviceType);
    NSString* displayName = config.displayName.empty()
        ? UIDevice.currentDevice.name
        : TCXIOSNs(config.displayName);
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
    TCXIOSFinishVoid(std::move(completion), tcx::ios::Result<void>::success());
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
        out.push_back({TCXIOSStr(peer.displayName), TCXIOSStr(peer.displayName), true});
    }
    return out;
}

- (void)send:(const std::vector<std::uint8_t>&)data completion:(tcx::ios::Completion<void>)completion {
    if (!session_ || session_.connectedPeers.count == 0) {
        TCXIOSFinishVoid(std::move(completion), tcx::ios::Result<void>::failure({
            tcx::ios::ErrorCode::InvalidState,
            "No connected multipeer peers.",
            0
        }));
        return;
    }
    NSError* error = nil;
    BOOL ok = [session_ sendData:TCXIOSData(data)
                         toPeers:session_.connectedPeers
                        withMode:MCSessionSendDataReliable
                           error:&error];
    TCXIOSFinishVoid(std::move(completion), ok
        ? tcx::ios::Result<void>::success()
        : tcx::ios::Result<void>::failure(TCXIOSNativeError(error, "Multipeer send failed.")));
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
    message.peer = {TCXIOSStr(peerID.displayName), TCXIOSStr(peerID.displayName), true};
    message.data = TCXIOSBytes(data);
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
