#pragma once

#include "Types.h"
#include "Operations.h"

#include <filesystem>
#include <string>
#include <vector>

namespace tcx::ios {

enum class PhotoMediaType {
    Image,
    Video,
    ImagesAndVideos
};

struct PickedPhoto {
    std::filesystem::path path;
    std::string typeIdentifier;
    PhotoMediaType mediaType = PhotoMediaType::Image;
    int pixelWidth = 0;
    int pixelHeight = 0;
};

struct PhotoPickerRequest {
    int selectionLimit = 1;
    bool preferCurrentRepresentation = true;
    PhotoMediaType mediaTypes = PhotoMediaType::Image;
};

struct PhotoSaveRequest {
    std::filesystem::path path;
    PhotoMediaType mediaType = PhotoMediaType::Image;
};

class Photos {
public:
    void pickPhotos(const PhotoPickerRequest& request, Completion<std::vector<PickedPhoto>> done);
    OperationHandle pickPhotosCancellable(const PhotoPickerRequest& request,
                                          Completion<std::vector<PickedPhoto>> done);
    void save(const PhotoSaveRequest& request, Completion<void> done);
    OperationHandle saveCancellable(const PhotoSaveRequest& request, Completion<void> done);
};

Photos& photos();

std::string toString(PhotoMediaType mediaType);

} // namespace tcx::ios
