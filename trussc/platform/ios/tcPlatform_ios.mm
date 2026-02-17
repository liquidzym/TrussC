// =============================================================================
// iOS platform-specific functions
// =============================================================================

#include "TrussC.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IOS

#import <UIKit/UIKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "sokol_app.h"

namespace trussc {
namespace platform {

float getDisplayScaleFactor() {
    return (float)[UIScreen mainScreen].scale;
}

void setWindowSize(int width, int height) {
    // no-op on iOS (window size is fixed to screen size)
    (void)width;
    (void)height;
}

std::string getExecutablePath() {
    NSString* path = [[NSBundle mainBundle] executablePath];
    return std::string([path UTF8String]);
}

std::string getExecutableDir() {
    NSString* path = [[NSBundle mainBundle] executablePath];
    NSString* dir = [path stringByDeletingLastPathComponent];
    return std::string([dir UTF8String]) + "/";
}

// ---------------------------------------------------------------------------
// Screenshot (Metal API)
// ---------------------------------------------------------------------------

bool captureWindow(Pixels& outPixels) {
    sapp_swapchain sc = sapp_get_swapchain();
    id<CAMetalDrawable> drawable = (__bridge id<CAMetalDrawable>)sc.metal.current_drawable;
    if (!drawable) {
        logError() << "[Screenshot] Failed to get Metal drawable";
        return false;
    }

    id<MTLTexture> texture = drawable.texture;
    if (!texture) {
        logError() << "[Screenshot] Failed to get Metal texture";
        return false;
    }

    NSUInteger width = texture.width;
    NSUInteger height = texture.height;

    outPixels.allocate((int)width, (int)height, 4);

    MTLRegion region = MTLRegionMake2D(0, 0, width, height);
    NSUInteger bytesPerRow = width * 4;

    [texture getBytes:outPixels.getData()
          bytesPerRow:bytesPerRow
           fromRegion:region
          mipmapLevel:0];

    // BGRA -> RGBA conversion
    unsigned char* data = outPixels.getData();
    for (NSUInteger i = 0; i < width * height; i++) {
        unsigned char temp = data[i * 4 + 0];
        data[i * 4 + 0] = data[i * 4 + 2];
        data[i * 4 + 2] = temp;
    }

    return true;
}

bool saveScreenshot(const std::filesystem::path& path) {
    Pixels pixels;
    if (!captureWindow(pixels)) {
        return false;
    }

    int width = pixels.getWidth();
    int height = pixels.getHeight();
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

    CGContextRef context = CGBitmapContextCreate(
        pixels.getData(),
        width, height,
        8,
        width * 4,
        colorSpace,
        kCGImageAlphaPremultipliedLast | (CGBitmapInfo)kCGBitmapByteOrder32Big
    );

    if (!context) {
        CGColorSpaceRelease(colorSpace);
        logError() << "[Screenshot] Failed to create CGContext";
        return false;
    }

    CGImageRef cgImage = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    CGColorSpaceRelease(colorSpace);

    if (!cgImage) {
        logError() << "[Screenshot] Failed to create CGImage";
        return false;
    }

    // Use UIImage + PNG/JPEG representation
    UIImage* image = [UIImage imageWithCGImage:cgImage];
    CGImageRelease(cgImage);

    if (!image) {
        logError() << "[Screenshot] Failed to create UIImage";
        return false;
    }

    NSData* data = nil;
    std::string ext = path.extension().string();
    if (ext == ".jpg" || ext == ".jpeg") {
        data = UIImageJPEGRepresentation(image, 0.9);
    } else {
        data = UIImagePNGRepresentation(image);
    }

    if (!data) {
        logError() << "[Screenshot] Failed to create image data";
        return false;
    }

    NSString* nsPath = [NSString stringWithUTF8String:path.c_str()];
    BOOL success = [data writeToFile:nsPath atomically:YES];

    if (success) {
        logVerbose() << "[Screenshot] Saved: " << path;
    } else {
        logError() << "[Screenshot] Failed to save: " << path;
    }

    return success;
}

} // namespace platform
} // namespace trussc

#endif // TARGET_OS_IOS
#endif // __APPLE__
