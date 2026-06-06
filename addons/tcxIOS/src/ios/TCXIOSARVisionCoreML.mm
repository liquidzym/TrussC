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

namespace {

ARSession* gARSession = nil;
id gARDelegate = nil;
std::mutex gTCXIOSV03ARMutex;
tcx::ios::ARFrameInfo gTCXIOSV03LatestARFrame;

} // namespace

@interface TCXIOSARDelegate : NSObject <ARSessionDelegate>
@end

@implementation TCXIOSARDelegate
- (void)session:(ARSession*)session didUpdateFrame:(ARFrame*)frame {
    CVPixelBufferRef pixelBuffer = frame.capturedImage;
    tcx::ios::ARFrameInfo info;
    info.timestampSeconds = frame.timestamp;
    info.cameraImageWidth = static_cast<int>(CVPixelBufferGetWidth(pixelBuffer));
    info.cameraImageHeight = static_cast<int>(CVPixelBufferGetHeight(pixelBuffer));
    std::lock_guard<std::mutex> lock(gTCXIOSV03ARMutex);
    gTCXIOSV03LatestARFrame = info;
}
@end

namespace tcx::ios::detail {
bool platformARWorldTrackingSupported() {
    return ARWorldTrackingConfiguration.isSupported;
}

void platformStartARSession(const ARSessionConfig& config, Completion<void> done) {
    if (!ARWorldTrackingConfiguration.isSupported) {
        TCXIOSV03FinishVoid(std::move(done), Result<void>::failure({ErrorCode::Unavailable, "AR world tracking is not supported on this device.", 0}));
        return;
    }
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!gARSession) {
            gARSession = [[ARSession alloc] init];
            gARDelegate = [[TCXIOSARDelegate alloc] init];
            gARSession.delegate = gARDelegate;
        }
        ARWorldTrackingConfiguration* configuration = [[ARWorldTrackingConfiguration alloc] init];
        if (config.planeDetection) {
            configuration.planeDetection = ARPlaneDetectionHorizontal | ARPlaneDetectionVertical;
        }
        [gARSession runWithConfiguration:configuration];
        TCXIOSV03FinishVoid(std::move(done), Result<void>::success());
    });
}

void platformStopARSession() {
    dispatch_async(dispatch_get_main_queue(), ^{
        [gARSession pause];
    });
}

bool platformLatestARFrame(ARFrameInfo& out) {
    std::lock_guard<std::mutex> lock(gTCXIOSV03ARMutex);
    if (gTCXIOSV03LatestARFrame.timestampSeconds <= 0.0) return false;
    out = gTCXIOSV03LatestARFrame;
    return true;
}

void platformDetectVisionRectangles(const std::filesystem::path& imagePath,
                                    Completion<std::vector<VisionRectangle>> done) {
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSURL* url = [NSURL fileURLWithPath:TCXIOSV03Ns(imagePath.string())];
        CIImage* image = [CIImage imageWithContentsOfURL:url];
        if (!image) {
            TCXIOSV03Finish(std::move(done), Result<std::vector<VisionRectangle>>::failure({ErrorCode::InvalidArgument, "Could not load image for Vision request.", 0}));
            return;
        }
        VNDetectRectanglesRequest* request = [[VNDetectRectanglesRequest alloc] init];
        VNImageRequestHandler* handler = [[VNImageRequestHandler alloc] initWithCIImage:image options:@{}];
        NSError* error = nil;
        BOOL ok = [handler performRequests:@[request] error:&error];
        if (!ok) {
            TCXIOSV03Finish(std::move(done), Result<std::vector<VisionRectangle>>::failure(TCXIOSV03NativeError(error, "Vision rectangle detection failed.")));
            return;
        }
        std::vector<VisionRectangle> rectangles;
        for (VNRectangleObservation* observation in request.results) {
            CGRect box = observation.boundingBox;
            rectangles.push_back({
                static_cast<float>(box.origin.x),
                static_cast<float>(box.origin.y),
                static_cast<float>(box.size.width),
                static_cast<float>(box.size.height),
                observation.confidence
            });
        }
        TCXIOSV03Finish(std::move(done), Result<std::vector<VisionRectangle>>::success(std::move(rectangles)));
    });
}

Result<CoreMLModelInfo> platformInspectCoreMLModel(const std::filesystem::path& compiledModelPath) {
    NSURL* url = [NSURL fileURLWithPath:TCXIOSV03Ns(compiledModelPath.string())];
    NSError* error = nil;
    MLModel* model = [MLModel modelWithContentsOfURL:url error:&error];
    if (!model) {
        return Result<CoreMLModelInfo>::failure(TCXIOSV03NativeError(error, "CoreML model load failed."));
    }
    return Result<CoreMLModelInfo>::success({compiledModelPath, true});
}

} // namespace tcx::ios::detail
