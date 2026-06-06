#include "TCXIOSBridgeSupport.h"

#import <AVFoundation/AVFoundation.h>
#import <Photos/Photos.h>
#import <PhotosUI/PhotosUI.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <UIKit/UIKit.h>

#include <cmath>
#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

@interface TCXIOSDocumentImportDelegate : NSObject <UIDocumentPickerDelegate>
- (instancetype)initWithCopyIntoApp:(bool)copyIntoApp
                         completion:(tcx::ios::Completion<std::vector<tcx::ios::PickedFile>>)completion;
@end

@implementation TCXIOSDocumentImportDelegate {
    tcx::ios::Completion<std::vector<tcx::ios::PickedFile>> completion_;
    bool copyIntoApp_;
}

- (instancetype)initWithCopyIntoApp:(bool)copyIntoApp
                         completion:(tcx::ios::Completion<std::vector<tcx::ios::PickedFile>>)completion {
    self = [super init];
    if (self) {
        copyIntoApp_ = copyIntoApp;
        completion_ = std::move(completion);
    }
    return self;
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
    [controller dismissViewControllerAnimated:YES completion:nil];
    TCXIOSReleaseDelegate(self);
    TCXIOSFinish(std::move(completion_), tcx::ios::Result<std::vector<tcx::ios::PickedFile>>::failure({
        tcx::ios::ErrorCode::Cancelled,
        "Document picker was cancelled.",
        0
    }));
}

- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    [controller dismissViewControllerAnimated:YES completion:nil];
    TCXIOSReleaseDelegate(self);

    std::vector<tcx::ios::PickedFile> picked;
    for (NSURL* url in urls) {
        BOOL scoped = [url startAccessingSecurityScopedResource];
        if (!copyIntoApp_) {
            NSString* typeIdentifier = nil;
            [url getResourceValue:&typeIdentifier forKey:NSURLTypeIdentifierKey error:nil];
            tcx::ios::PickedFile file;
            file.localPath = std::filesystem::path(TCXIOSStr(url.path));
            file.contentType = TCXIOSStr(typeIdentifier);
            file.copiedIntoSandbox = false;
            file.path = file.localPath;
            file.uti = file.contentType;
            file.securityScoped = scoped == YES;
            picked.push_back(std::move(file));
            continue;
        }

        NSError* error = nil;
        NSURL* copiedURL = TCXIOSTemporaryCopyURL(url, @"tcxIOS-imports", &error);
        if (scoped) [url stopAccessingSecurityScopedResource];
        if (!copiedURL) {
            TCXIOSFinish(std::move(completion_), tcx::ios::Result<std::vector<tcx::ios::PickedFile>>::failure(
                TCXIOSNativeError(error, "Failed to copy imported document into the app sandbox.")));
            return;
        }

        NSString* typeIdentifier = nil;
        [copiedURL getResourceValue:&typeIdentifier forKey:NSURLTypeIdentifierKey error:nil];
        tcx::ios::PickedFile file;
        file.localPath = std::filesystem::path(TCXIOSStr(copiedURL.path));
        file.contentType = TCXIOSStr(typeIdentifier);
        file.copiedIntoSandbox = true;
        file.path = file.localPath;
        file.uti = file.contentType;
        file.securityScoped = false;
        picked.push_back(std::move(file));
    }

    TCXIOSFinish(std::move(completion_), tcx::ios::Result<std::vector<tcx::ios::PickedFile>>::success(std::move(picked)));
}

@end

@interface TCXIOSDocumentExportDelegate : NSObject <UIDocumentPickerDelegate>
- (instancetype)initWithCompletion:(tcx::ios::Completion<void>)completion;
@end

@implementation TCXIOSDocumentExportDelegate {
    tcx::ios::Completion<void> completion_;
}

- (instancetype)initWithCompletion:(tcx::ios::Completion<void>)completion {
    self = [super init];
    if (self) completion_ = std::move(completion);
    return self;
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller {
    [controller dismissViewControllerAnimated:YES completion:nil];
    TCXIOSReleaseDelegate(self);
    TCXIOSFinishVoid(std::move(completion_), tcx::ios::Result<void>::failure({
        tcx::ios::ErrorCode::Cancelled,
        "Document export was cancelled.",
        0
    }));
}

- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls {
    [controller dismissViewControllerAnimated:YES completion:nil];
    TCXIOSReleaseDelegate(self);
    TCXIOSFinishVoid(std::move(completion_), tcx::ios::Result<void>::success());
}

@end

@interface TCXIOSPhotoPickerDelegate : NSObject <PHPickerViewControllerDelegate>
- (instancetype)initWithMediaTypes:(tcx::ios::PhotoMediaType)mediaTypes
                     limitedLibrary:(bool)limitedLibrary
                        completion:(tcx::ios::Completion<std::vector<tcx::ios::PickedPhoto>>)completion;
@end

@implementation TCXIOSPhotoPickerDelegate {
    tcx::ios::Completion<std::vector<tcx::ios::PickedPhoto>> completion_;
    tcx::ios::PhotoMediaType mediaTypes_;
    bool limitedLibrary_;
}

- (instancetype)initWithMediaTypes:(tcx::ios::PhotoMediaType)mediaTypes
                     limitedLibrary:(bool)limitedLibrary
                        completion:(tcx::ios::Completion<std::vector<tcx::ios::PickedPhoto>>)completion {
    self = [super init];
    if (self) {
        mediaTypes_ = mediaTypes;
        limitedLibrary_ = limitedLibrary;
        completion_ = std::move(completion);
    }
    return self;
}

- (void)picker:(PHPickerViewController*)picker didFinishPicking:(NSArray<PHPickerResult*>*)results {
    [picker dismissViewControllerAnimated:YES completion:nil];
    if (results.count == 0) {
        TCXIOSReleaseDelegate(self);
        TCXIOSFinish(std::move(completion_), tcx::ios::Result<std::vector<tcx::ios::PickedPhoto>>::failure({
            tcx::ios::ErrorCode::Cancelled,
            "Photo picker was cancelled.",
            0
        }));
        return;
    }

    dispatch_group_t group = dispatch_group_create();
    auto photos = std::make_shared<std::vector<tcx::ios::PickedPhoto>>();
    auto firstError = std::make_shared<tcx::ios::Error>();
    auto mutex = std::make_shared<std::mutex>();

    for (PHPickerResult* result in results) {
        NSItemProvider* provider = result.itemProvider;
        const bool wantsVideo = mediaTypes_ == tcx::ios::PhotoMediaType::Video ||
                                mediaTypes_ == tcx::ios::PhotoMediaType::ImagesAndVideos;
        const bool wantsImage = mediaTypes_ == tcx::ios::PhotoMediaType::Image ||
                                mediaTypes_ == tcx::ios::PhotoMediaType::ImagesAndVideos;
        NSString* typeIdentifier = nil;
        tcx::ios::PhotoMediaType pickedMediaType = tcx::ios::PhotoMediaType::Image;
        if (wantsImage && [provider hasItemConformingToTypeIdentifier:UTTypeImage.identifier]) {
            typeIdentifier = UTTypeImage.identifier;
            pickedMediaType = tcx::ios::PhotoMediaType::Image;
        } else if (wantsVideo && [provider hasItemConformingToTypeIdentifier:UTTypeMovie.identifier]) {
            typeIdentifier = UTTypeMovie.identifier;
            pickedMediaType = tcx::ios::PhotoMediaType::Video;
        }
        if (!typeIdentifier) continue;
        if (![provider hasItemConformingToTypeIdentifier:typeIdentifier]) continue;

        dispatch_group_enter(group);
        [provider loadFileRepresentationForTypeIdentifier:typeIdentifier completionHandler:^(NSURL* url, NSError* error) {
            if (!url || error) {
                std::lock_guard<std::mutex> lock(*mutex);
                if (firstError->code == tcx::ios::ErrorCode::None) {
                    *firstError = TCXIOSNativeError(error, "Failed to load photo representation.");
                }
                dispatch_group_leave(group);
                return;
            }

            NSError* copyError = nil;
            NSURL* copiedURL = TCXIOSTemporaryCopyURL(url, @"tcxIOS-photos", &copyError);
            if (!copiedURL) {
                std::lock_guard<std::mutex> lock(*mutex);
                if (firstError->code == tcx::ios::ErrorCode::None) {
                    *firstError = TCXIOSNativeError(copyError, "Failed to copy picked photo into the app sandbox.");
                }
                dispatch_group_leave(group);
                return;
            }

            UIImage* image = pickedMediaType == tcx::ios::PhotoMediaType::Image
                ? [UIImage imageWithContentsOfFile:copiedURL.path]
                : nil;
            int pixelWidth = image ? static_cast<int>(image.size.width * image.scale) : 0;
            int pixelHeight = image ? static_cast<int>(image.size.height * image.scale) : 0;
            NSString* copiedTypeIdentifier = nil;
            [copiedURL getResourceValue:&copiedTypeIdentifier forKey:NSURLTypeIdentifierKey error:nil];
            NSNumber* fileSize = nil;
            [copiedURL getResourceValue:&fileSize forKey:NSURLFileSizeKey error:nil];
            double durationSeconds = 0.0;
            if (pickedMediaType == tcx::ios::PhotoMediaType::Video) {
                AVURLAsset* asset = [AVURLAsset URLAssetWithURL:copiedURL options:nil];
                const double seconds = CMTimeGetSeconds(asset.duration);
                durationSeconds = std::isfinite(seconds) && seconds > 0.0 ? seconds : 0.0;
            }

            std::lock_guard<std::mutex> lock(*mutex);
            tcx::ios::PickedPhoto photo;
            photo.path = std::filesystem::path(TCXIOSStr(copiedURL.path));
            photo.typeIdentifier = TCXIOSStr(copiedTypeIdentifier.length > 0 ? copiedTypeIdentifier : typeIdentifier);
            photo.filename = TCXIOSStr(copiedURL.lastPathComponent);
            photo.mediaType = pickedMediaType;
            photo.pixelWidth = pixelWidth;
            photo.pixelHeight = pixelHeight;
            photo.fileSize = fileSize ? static_cast<std::uint64_t>(fileSize.unsignedLongLongValue) : 0;
            photo.durationSeconds = durationSeconds;
            photo.limitedLibrary = limitedLibrary_;
            photos->push_back(std::move(photo));
            dispatch_group_leave(group);
        }];
    }

    dispatch_group_notify(group, dispatch_get_main_queue(), ^{
        TCXIOSReleaseDelegate(self);
        if (firstError->code != tcx::ios::ErrorCode::None) {
            TCXIOSFinish(std::move(completion_), tcx::ios::Result<std::vector<tcx::ios::PickedPhoto>>::failure(*firstError));
            return;
        }
        TCXIOSFinish(std::move(completion_), tcx::ios::Result<std::vector<tcx::ios::PickedPhoto>>::success(std::move(*photos)));
    });
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

std::filesystem::path platformAppDirectoryPath(AppDirectory directory) {
    switch (directory) {
        case AppDirectory::Documents: {
            NSArray<NSString*>* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
            return paths.count > 0 ? std::filesystem::path(str(paths.firstObject)) : std::filesystem::path();
        }
        case AppDirectory::Caches: {
            NSArray<NSString*>* paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
            return paths.count > 0 ? std::filesystem::path(str(paths.firstObject)) : std::filesystem::path();
        }
        case AppDirectory::Temporary:
            return std::filesystem::path(str(NSTemporaryDirectory()));
        case AppDirectory::ApplicationSupport: {
            NSArray<NSString*>* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
            return paths.count > 0 ? std::filesystem::path(str(paths.firstObject)) : std::filesystem::path();
        }
    }
    return {};
}

void platformImportFiles(const ImportFileRequest& request, Completion<std::vector<PickedFile>> done) {
    dispatch_async(dispatch_get_main_queue(), ^{
        UIViewController* viewController = presenter();
        if (!viewController) {
            finish(std::move(done), Result<std::vector<PickedFile>>::failure({ErrorCode::InvalidState, "No active view controller for document picker.", 0}));
            return;
        }

        UIDocumentPickerViewController* picker =
            [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:TCXIOSContentTypes(request.contentTypes)
                                                                        asCopy:(request.copyIntoApp ? YES : NO)];
        picker.allowsMultipleSelection = request.allowMultiple;
        TCXIOSDocumentImportDelegate* delegate =
            [[TCXIOSDocumentImportDelegate alloc] initWithCopyIntoApp:request.copyIntoApp
                                                           completion:std::move(done)];
        picker.delegate = delegate;
        TCXIOSRetainDelegate(delegate);
        [viewController presentViewController:picker animated:YES completion:nil];
    });
}

void platformExportFile(const ExportFileRequest& request, Completion<void> done) {
    dispatch_async(dispatch_get_main_queue(), ^{
        UIViewController* viewController = presenter();
        if (!viewController) {
            finishVoid(std::move(done), Result<void>::failure({ErrorCode::InvalidState, "No active view controller for document export.", 0}));
            return;
        }

        NSURL* sourceURL = [NSURL fileURLWithPath:TCXIOSNs(request.path.string())];
        NSFileManager* fileManager = [NSFileManager defaultManager];
        if (![fileManager fileExistsAtPath:sourceURL.path]) {
            finishVoid(std::move(done), Result<void>::failure({ErrorCode::InvalidArgument, "Export file does not exist.", 0}));
            return;
        }

        NSURL* exportURL = sourceURL;
        if (!request.suggestedName.empty()) {
            NSError* error = nil;
            NSString* directory = [NSTemporaryDirectory() stringByAppendingPathComponent:@"tcxIOS-exports"];
            if (![fileManager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:&error]) {
                finishVoid(std::move(done), Result<void>::failure(nativeError(error, "Failed to create export staging directory.")));
                return;
            }
            exportURL = [NSURL fileURLWithPath:[directory stringByAppendingPathComponent:TCXIOSNs(request.suggestedName)]];
            [fileManager removeItemAtURL:exportURL error:nil];
            if (![fileManager copyItemAtURL:sourceURL toURL:exportURL error:&error]) {
                finishVoid(std::move(done), Result<void>::failure(nativeError(error, "Failed to stage export file.")));
                return;
            }
        }

        UIDocumentPickerViewController* picker =
            [[UIDocumentPickerViewController alloc] initForExportingURLs:@[exportURL] asCopy:YES];
        TCXIOSDocumentExportDelegate* delegate =
            [[TCXIOSDocumentExportDelegate alloc] initWithCompletion:std::move(done)];
        picker.delegate = delegate;
        TCXIOSRetainDelegate(delegate);
        [viewController presentViewController:picker animated:YES completion:nil];
    });
}

void platformStopAccessingFile(const PickedFile& file) {
    if (!file.securityScoped || file.path.empty()) return;
    NSURL* url = [NSURL fileURLWithPath:ns(file.path.string())];
    [url stopAccessingSecurityScopedResource];
}

void platformPickPhotos(const PhotoPickerRequest& request, Completion<std::vector<PickedPhoto>> done) {
    dispatch_async(dispatch_get_main_queue(), ^{
        UIViewController* viewController = presenter();
        if (!viewController) {
            finish(std::move(done), Result<std::vector<PickedPhoto>>::failure({ErrorCode::InvalidState, "No active view controller for photo picker.", 0}));
            return;
        }

        PHPickerConfiguration* configuration =
            [[PHPickerConfiguration alloc] initWithPhotoLibrary:PHPhotoLibrary.sharedPhotoLibrary];
        configuration.selectionLimit = request.selectionLimit < 0 ? 0 : request.selectionLimit;
        switch (request.mediaTypes) {
            case PhotoMediaType::Image:
                configuration.filter = PHPickerFilter.imagesFilter;
                break;
            case PhotoMediaType::Video:
                configuration.filter = PHPickerFilter.videosFilter;
                break;
            case PhotoMediaType::ImagesAndVideos:
                configuration.filter = [PHPickerFilter anyFilterMatchingSubfilters:@[
                    PHPickerFilter.imagesFilter,
                    PHPickerFilter.videosFilter
                ]];
                break;
        }
        configuration.preferredAssetRepresentationMode = request.preferCurrentRepresentation
            ? PHPickerConfigurationAssetRepresentationModeCurrent
            : PHPickerConfigurationAssetRepresentationModeAutomatic;
        const bool limitedLibrary =
            [PHPhotoLibrary authorizationStatusForAccessLevel:PHAccessLevelReadWrite] == PHAuthorizationStatusLimited;

        PHPickerViewController* picker = [[PHPickerViewController alloc] initWithConfiguration:configuration];
        TCXIOSPhotoPickerDelegate* delegate =
            [[TCXIOSPhotoPickerDelegate alloc] initWithMediaTypes:request.mediaTypes
                                                   limitedLibrary:limitedLibrary
                                                       completion:std::move(done)];
        picker.delegate = delegate;
        TCXIOSRetainDelegate(delegate);
        [viewController presentViewController:picker animated:YES completion:nil];
    });
}

void platformSavePhoto(const PhotoSaveRequest& request, Completion<void> done) {
    NSURL* url = [NSURL fileURLWithPath:ns(request.path.string())];
    if (!url) {
        finishVoid(std::move(done), Result<void>::failure({ErrorCode::InvalidArgument, "Photo save path is invalid.", 0}));
        return;
    }
    [PHPhotoLibrary.sharedPhotoLibrary performChanges:^{
        switch (request.mediaType) {
            case PhotoMediaType::Image:
                [PHAssetChangeRequest creationRequestForAssetFromImageAtFileURL:url];
                break;
            case PhotoMediaType::Video:
                [PHAssetChangeRequest creationRequestForAssetFromVideoAtFileURL:url];
                break;
            case PhotoMediaType::ImagesAndVideos:
                [PHAssetChangeRequest creationRequestForAssetFromImageAtFileURL:url];
                break;
        }
    } completionHandler:^(BOOL success, NSError* error) {
        if (success) {
            finishVoid(std::move(done), Result<void>::success());
        } else {
            finishVoid(std::move(done), Result<void>::failure(nativeError(error, "Failed to save media to Photos.")));
        }
    }];
}


} // namespace tcx::ios::detail
