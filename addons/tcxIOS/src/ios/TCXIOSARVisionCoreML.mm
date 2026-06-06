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

namespace {

ARSession* gARSession = nil;
id gARDelegate = nil;
std::mutex gTCXIOSARMutex;
tcx::ios::ARFrameInfo gTCXIOSLatestARFrame;

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
    std::lock_guard<std::mutex> lock(gTCXIOSARMutex);
    gTCXIOSLatestARFrame = info;
}
@end

namespace tcx::ios::detail {
bool platformARWorldTrackingSupported() {
    return ARWorldTrackingConfiguration.isSupported;
}

void platformStartARSession(const ARSessionConfig& config, Completion<void> done) {
    if (!ARWorldTrackingConfiguration.isSupported) {
        TCXIOSFinishVoid(std::move(done), Result<void>::failure({ErrorCode::Unavailable, "AR world tracking is not supported on this device.", 0}));
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
        TCXIOSFinishVoid(std::move(done), Result<void>::success());
    });
}

void platformStopARSession() {
    dispatch_async(dispatch_get_main_queue(), ^{
        [gARSession pause];
    });
}

bool platformLatestARFrame(ARFrameInfo& out) {
    std::lock_guard<std::mutex> lock(gTCXIOSARMutex);
    if (gTCXIOSLatestARFrame.timestampSeconds <= 0.0) return false;
    out = gTCXIOSLatestARFrame;
    return true;
}

void platformDetectVisionRectangles(const std::filesystem::path& imagePath,
                                    Completion<std::vector<VisionRectangle>> done) {
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSURL* url = [NSURL fileURLWithPath:TCXIOSNs(imagePath.string())];
        CIImage* image = [CIImage imageWithContentsOfURL:url];
        if (!image) {
            TCXIOSFinish(std::move(done), Result<std::vector<VisionRectangle>>::failure({ErrorCode::InvalidArgument, "Could not load image for Vision request.", 0}));
            return;
        }
        VNDetectRectanglesRequest* request = [[VNDetectRectanglesRequest alloc] init];
        VNImageRequestHandler* handler = [[VNImageRequestHandler alloc] initWithCIImage:image options:@{}];
        NSError* error = nil;
        BOOL ok = [handler performRequests:@[request] error:&error];
        if (!ok) {
            TCXIOSFinish(std::move(done), Result<std::vector<VisionRectangle>>::failure(TCXIOSNativeError(error, "Vision rectangle detection failed.")));
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
        TCXIOSFinish(std::move(done), Result<std::vector<VisionRectangle>>::success(std::move(rectangles)));
    });
}

Result<CoreMLModelInfo> platformInspectCoreMLModel(const std::filesystem::path& compiledModelPath) {
    NSURL* url = [NSURL fileURLWithPath:TCXIOSNs(compiledModelPath.string())];
    NSError* error = nil;
    MLModel* model = [MLModel modelWithContentsOfURL:url error:&error];
    if (!model) {
        return Result<CoreMLModelInfo>::failure(TCXIOSNativeError(error, "CoreML model load failed."));
    }
    return Result<CoreMLModelInfo>::success({compiledModelPath, true});
}

} // namespace tcx::ios::detail
