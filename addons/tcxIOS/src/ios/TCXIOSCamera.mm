#include "TCXIOSBridgeSupport.h"

#import <AVFoundation/AVFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>


namespace {

dispatch_queue_t TCXIOSCameraQueue() {
    static dispatch_queue_t queue = dispatch_queue_create("org.trussc.tcxios.camera", DISPATCH_QUEUE_SERIAL);
    return queue;
}

std::mutex gTCXIOSCameraMutex;
tcx::ios::CameraFrame gTCXIOSLatestCameraFrame;
std::shared_ptr<const std::vector<std::uint8_t>> gTCXIOSLatestCameraStorage;
std::deque<std::shared_ptr<const std::vector<std::uint8_t>>> gTCXIOSCameraRing;
int gTCXIOSCameraRingCapacity = 3;
std::atomic_uint64_t gTCXIOSCameraFrameId{0};
std::atomic_uint64_t gTCXIOSCameraDroppedFrames{0};
std::atomic_bool gTCXIOSCameraRunning{false};
AVCaptureSession* gTCXIOSCameraSession = nil;
AVCaptureVideoDataOutput* gTCXIOSCameraOutput = nil;
id gTCXIOSCameraDelegate = nil;

} // namespace

@interface TCXIOSCameraVideoDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
@end

@implementation TCXIOSCameraVideoDelegate

- (void)captureOutput:(AVCaptureOutput*)output
 didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
        fromConnection:(AVCaptureConnection*)connection {
    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!imageBuffer) return;

    CVPixelBufferRef pixelBuffer = (CVPixelBufferRef)imageBuffer;
    OSType format = CVPixelBufferGetPixelFormatType(pixelBuffer);
    if (format != kCVPixelFormatType_32BGRA) return;

    CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    const int width = static_cast<int>(CVPixelBufferGetWidth(pixelBuffer));
    const int height = static_cast<int>(CVPixelBufferGetHeight(pixelBuffer));
    const int sourceBytesPerRow = static_cast<int>(CVPixelBufferGetBytesPerRow(pixelBuffer));
    const int destinationBytesPerRow = width * 4;
    const std::size_t destinationSize = static_cast<std::size_t>(destinationBytesPerRow) *
                                        static_cast<std::size_t>(height);
    const std::uint8_t* source = static_cast<const std::uint8_t*>(CVPixelBufferGetBaseAddress(pixelBuffer));
    if (!source || width <= 0 || height <= 0 || sourceBytesPerRow < destinationBytesPerRow) {
        CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
        return;
    }

    auto storage = std::make_shared<std::vector<std::uint8_t>>(destinationSize);
    tcx::ios::CameraFrame frame;
    frame.frameId = gTCXIOSCameraFrameId.fetch_add(1) + 1;
    frame.droppedFrameCount = gTCXIOSCameraDroppedFrames.load();
    frame.width = width;
    frame.height = height;
    frame.bytesPerRow = destinationBytesPerRow;
    frame.pixelFormat = tcx::ios::CameraPixelFormat::BGRA8;
    frame.timestampSeconds = CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sampleBuffer));
    for (int y = 0; y < height; ++y) {
        std::memcpy(storage->data() + static_cast<std::size_t>(y) * destinationBytesPerRow,
                    source + static_cast<std::size_t>(y) * sourceBytesPerRow,
                    destinationBytesPerRow);
    }
    frame.data = *storage;

    CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);

    std::lock_guard<std::mutex> lock(gTCXIOSCameraMutex);
    gTCXIOSLatestCameraFrame = std::move(frame);
    gTCXIOSLatestCameraStorage = storage;
    gTCXIOSCameraRing.push_back(std::move(storage));
    while (static_cast<int>(gTCXIOSCameraRing.size()) > gTCXIOSCameraRingCapacity) {
        gTCXIOSCameraRing.pop_front();
    }
}

- (void)captureOutput:(AVCaptureOutput*)output
 didDropSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection*)connection {
    (void)output;
    (void)sampleBuffer;
    (void)connection;
    gTCXIOSCameraDroppedFrames.fetch_add(1);
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

AVCaptureDevicePosition cameraPosition(CameraDevicePosition position) {
    switch (position) {
        case CameraDevicePosition::Front: return AVCaptureDevicePositionFront;
        case CameraDevicePosition::Back: return AVCaptureDevicePositionBack;
        case CameraDevicePosition::External:
        case CameraDevicePosition::Unspecified: return AVCaptureDevicePositionUnspecified;
    }
    return AVCaptureDevicePositionUnspecified;
}

CameraDevicePosition cameraDevicePosition(AVCaptureDevicePosition position) {
    switch (position) {
        case AVCaptureDevicePositionFront: return CameraDevicePosition::Front;
        case AVCaptureDevicePositionBack: return CameraDevicePosition::Back;
        case AVCaptureDevicePositionUnspecified: return CameraDevicePosition::Unspecified;
    }
    return CameraDevicePosition::Unspecified;
}

AVCaptureVideoOrientation cameraOrientation(CameraOrientation orientation) {
    switch (orientation) {
        case CameraOrientation::Portrait: return AVCaptureVideoOrientationPortrait;
        case CameraOrientation::PortraitUpsideDown: return AVCaptureVideoOrientationPortraitUpsideDown;
        case CameraOrientation::LandscapeLeft: return AVCaptureVideoOrientationLandscapeLeft;
        case CameraOrientation::LandscapeRight: return AVCaptureVideoOrientationLandscapeRight;
        case CameraOrientation::Unspecified: return AVCaptureVideoOrientationPortrait;
    }
    return AVCaptureVideoOrientationPortrait;
}

std::vector<CameraFormat> cameraFormats(AVCaptureDevice* device) {
    std::vector<CameraFormat> formats;
    for (AVCaptureDeviceFormat* nativeFormat in device.formats) {
        CMVideoDimensions dimensions =
            CMVideoFormatDescriptionGetDimensions(nativeFormat.formatDescription);
        CameraFormat format;
        format.width = dimensions.width;
        format.height = dimensions.height;
        format.pixelFormat = CameraPixelFormat::BGRA8;
        for (AVFrameRateRange* range in nativeFormat.videoSupportedFrameRateRanges) {
            const int minFPS = static_cast<int>(std::floor(range.minFrameRate));
            const int maxFPS = static_cast<int>(std::ceil(range.maxFrameRate));
            if (format.minFramesPerSecond == 0 || minFPS < format.minFramesPerSecond) {
                format.minFramesPerSecond = minFPS;
            }
            if (maxFPS > format.maxFramesPerSecond) {
                format.maxFramesPerSecond = maxFPS;
            }
        }
        formats.push_back(format);
    }
    return formats;
}

AVCaptureDevice* cameraDeviceForConfig(const CameraConfig& config) {
    NSMutableArray<AVCaptureDeviceType>* deviceTypes = [NSMutableArray arrayWithArray:@[
        AVCaptureDeviceTypeBuiltInWideAngleCamera
    ]];
    if (@available(iOS 13.0, *)) {
        [deviceTypes addObject:AVCaptureDeviceTypeBuiltInUltraWideCamera];
        [deviceTypes addObject:AVCaptureDeviceTypeBuiltInDualWideCamera];
    }
    if (@available(iOS 17.0, *)) {
        [deviceTypes addObject:AVCaptureDeviceTypeExternal];
    }

    AVCaptureDeviceDiscoverySession* discovery =
        [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:deviceTypes
                                                                mediaType:AVMediaTypeVideo
                                                                 position:cameraPosition(config.position)];
    AVCaptureDevice* first = discovery.devices.firstObject;
    if (first) return first;

    if (config.position == CameraDevicePosition::External) {
        return [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    }
    return [AVCaptureDevice defaultDeviceWithDeviceType:AVCaptureDeviceTypeBuiltInWideAngleCamera
                                             mediaType:AVMediaTypeVideo
                                              position:cameraPosition(config.position)];
}


void platformStartCamera(const CameraConfig& config, Completion<void> done) {
    AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo];
    if (status == AVAuthorizationStatusDenied) {
        finishVoid(std::move(done), Result<void>::failure({ErrorCode::PermissionDenied, "Camera permission was denied.", 0}));
        return;
    }
    if (status == AVAuthorizationStatusRestricted) {
        finishVoid(std::move(done), Result<void>::failure({ErrorCode::PermissionRestricted, "Camera permission is restricted.", 0}));
        return;
    }
    if (status == AVAuthorizationStatusNotDetermined) {
        finishVoid(std::move(done), Result<void>::failure({ErrorCode::InvalidState, "Request Permission::Camera before starting camera capture.", 0}));
        return;
    }

    dispatch_async(TCXIOSCameraQueue(), ^{
        NSError* error = nil;
        AVCaptureDevice* device = cameraDeviceForConfig(config);
        if (!device) {
            finishVoid(std::move(done), Result<void>::failure({ErrorCode::Unavailable, "No video capture device is available.", 0}));
            return;
        }

        AVCaptureSession* session = [[AVCaptureSession alloc] init];
        if (config.width >= 1920 || config.height >= 1080) {
            session.sessionPreset = AVCaptureSessionPreset1920x1080;
        } else if (config.width >= 1280 || config.height >= 720) {
            session.sessionPreset = AVCaptureSessionPreset1280x720;
        } else {
            session.sessionPreset = AVCaptureSessionPreset640x480;
        }

        AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
        if (!input || ![session canAddInput:input]) {
            finishVoid(std::move(done), Result<void>::failure(nativeError(error, "Failed to create camera input.")));
            return;
        }
        [session addInput:input];

        AVCaptureVideoDataOutput* output = [[AVCaptureVideoDataOutput alloc] init];
        output.videoSettings = @{(id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA)};
        output.alwaysDiscardsLateVideoFrames = YES;
        TCXIOSCameraVideoDelegate* delegate = [[TCXIOSCameraVideoDelegate alloc] init];
        [output setSampleBufferDelegate:delegate queue:TCXIOSCameraQueue()];
        if (![session canAddOutput:output]) {
            finishVoid(std::move(done), Result<void>::failure({ErrorCode::NativeError, "Failed to add camera video output.", 0}));
            return;
        }
        [session addOutput:output];

        AVCaptureConnection* connection = [output connectionWithMediaType:AVMediaTypeVideo];
        if (connection) {
            if (connection.supportsVideoOrientation) {
                connection.videoOrientation = cameraOrientation(config.orientation);
            }
            if (connection.supportsVideoMirroring) {
                connection.videoMirrored = config.mirrored;
            }
        }

        if ([device lockForConfiguration:&error]) {
            if (config.framesPerSecond > 0) {
                CMTime frameDuration = CMTimeMake(1, config.framesPerSecond);
                device.activeVideoMinFrameDuration = frameDuration;
                device.activeVideoMaxFrameDuration = frameDuration;
            }
            [device unlockForConfiguration];
        }

        if (gTCXIOSCameraSession && gTCXIOSCameraSession.isRunning) {
            [gTCXIOSCameraSession stopRunning];
        }
        {
            std::lock_guard<std::mutex> lock(gTCXIOSCameraMutex);
            gTCXIOSLatestCameraFrame = {};
            gTCXIOSLatestCameraStorage.reset();
            gTCXIOSCameraRing.clear();
            gTCXIOSCameraRingCapacity = std::max(1, config.ringBufferCapacity);
        }
        gTCXIOSCameraFrameId.store(0);
        gTCXIOSCameraDroppedFrames.store(0);
        gTCXIOSCameraSession = session;
        gTCXIOSCameraOutput = output;
        gTCXIOSCameraDelegate = delegate;
        gTCXIOSCameraRunning.store(false);

        [session startRunning];
        const bool running = session.isRunning;
        gTCXIOSCameraRunning.store(running);
        if (running) {
            finishVoid(std::move(done), Result<void>::success());
        } else {
            finishVoid(std::move(done), Result<void>::failure({ErrorCode::NativeError, "AVCaptureSession did not start running.", 0}));
        }
    });
}

void platformStopCamera() {
    dispatch_async(TCXIOSCameraQueue(), ^{
        if (gTCXIOSCameraSession && gTCXIOSCameraSession.isRunning) {
            [gTCXIOSCameraSession stopRunning];
        }
        gTCXIOSCameraSession = nil;
        gTCXIOSCameraOutput = nil;
        gTCXIOSCameraDelegate = nil;
        gTCXIOSCameraRunning.store(false);
        std::lock_guard<std::mutex> lock(gTCXIOSCameraMutex);
        gTCXIOSLatestCameraFrame = {};
        gTCXIOSLatestCameraStorage.reset();
        gTCXIOSCameraRing.clear();
        gTCXIOSCameraFrameId.store(0);
        gTCXIOSCameraDroppedFrames.store(0);
    });
}

bool platformCameraIsRunning() {
    return gTCXIOSCameraRunning.load();
}

bool platformLatestCameraFrame(CameraFrame& out) {
    std::lock_guard<std::mutex> lock(gTCXIOSCameraMutex);
    if (gTCXIOSLatestCameraFrame.data.empty()) return false;
    out = gTCXIOSLatestCameraFrame;
    return true;
}

bool platformLatestCameraFrameView(CameraFrameView& out) {
    std::lock_guard<std::mutex> lock(gTCXIOSCameraMutex);
    if (!gTCXIOSLatestCameraStorage || gTCXIOSLatestCameraStorage->empty()) return false;
    out.frameId = gTCXIOSLatestCameraFrame.frameId;
    out.droppedFrameCount = gTCXIOSLatestCameraFrame.droppedFrameCount;
    out.width = gTCXIOSLatestCameraFrame.width;
    out.height = gTCXIOSLatestCameraFrame.height;
    out.bytesPerRow = gTCXIOSLatestCameraFrame.bytesPerRow;
    out.pixelFormat = gTCXIOSLatestCameraFrame.pixelFormat;
    out.timestampSeconds = gTCXIOSLatestCameraFrame.timestampSeconds;
    out.storage = gTCXIOSLatestCameraStorage;
    out.data = out.storage->data();
    out.size = out.storage->size();
    return true;
}

std::vector<CameraDeviceInfo> platformAvailableCameraDevices() {
    NSMutableArray<AVCaptureDeviceType>* deviceTypes = [NSMutableArray arrayWithArray:@[
        AVCaptureDeviceTypeBuiltInWideAngleCamera
    ]];
    if (@available(iOS 13.0, *)) {
        [deviceTypes addObject:AVCaptureDeviceTypeBuiltInUltraWideCamera];
        [deviceTypes addObject:AVCaptureDeviceTypeBuiltInDualWideCamera];
    }
    if (@available(iOS 17.0, *)) {
        [deviceTypes addObject:AVCaptureDeviceTypeExternal];
    }

    AVCaptureDeviceDiscoverySession* discovery =
        [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:deviceTypes
                                                                mediaType:AVMediaTypeVideo
                                                                 position:AVCaptureDevicePositionUnspecified];
    std::vector<CameraDeviceInfo> devices;
    for (AVCaptureDevice* device in discovery.devices) {
        CameraDeviceInfo info;
        info.identifier = TCXIOSStr(device.uniqueID);
        info.name = TCXIOSStr(device.localizedName);
        info.position = cameraDevicePosition(device.position);
        info.formats = cameraFormats(device);
        devices.push_back(std::move(info));
    }
    return devices;
}


} // namespace tcx::ios::detail
