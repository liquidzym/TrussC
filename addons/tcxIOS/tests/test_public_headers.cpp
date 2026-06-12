#include "test_common.h"

#include "tcxIOS.h"

#include <TrussC.h>

#include <filesystem>
#include <fstream>
#include <sstream>

using namespace tcx::ios;

TEST(public_include_compiles) {
    Error error = unavailableError("test");
    ASSERT_EQ(error.code, ErrorCode::Unavailable);

    AlertRequest alert;
    alert.title = "Title";
    alert.message = "Message";
    ASSERT_EQ(alert.buttons.size(), static_cast<std::size_t>(1));

    CameraConfig cameraConfig;
    ASSERT_EQ(cameraConfig.pixelFormat, CameraPixelFormat::BGRA8);
    cameraConfig.position = CameraDevicePosition::Front;
    cameraConfig.orientation = CameraOrientation::Portrait;
    cameraConfig.mirrored = true;
    ASSERT_EQ(toString(cameraConfig.position), std::string("front"));

    CameraFormat cameraFormat;
    cameraFormat.width = 1920;
    cameraFormat.height = 1080;
    cameraFormat.minFramesPerSecond = 24;
    cameraFormat.maxFramesPerSecond = 60;
    ASSERT_EQ(cameraFormat.maxFramesPerSecond, 60);

    CameraDeviceInfo cameraDevice;
    cameraDevice.identifier = "front";
    cameraDevice.position = CameraDevicePosition::Front;
    cameraDevice.formats.push_back(cameraFormat);
    ASSERT_EQ(cameraDevice.formats.size(), static_cast<std::size_t>(1));

    CameraFrame cameraFrame;
    ASSERT_EQ(cameraFrame.frameId, static_cast<std::uint64_t>(0));
    ASSERT_EQ(cameraFrame.droppedFrameCount, static_cast<std::uint64_t>(0));
    cameraFrame.width = 1;
    cameraFrame.height = 1;
    cameraFrame.bytesPerRow = 4;
    cameraFrame.pixelFormat = CameraPixelFormat::BGRA8;
    cameraFrame.data = {1, 2, 3, 4};
    trussc::Pixels cameraPixels;
    ASSERT_TRUE(copyCameraFrameToPixels(cameraFrame, cameraPixels));
    ASSERT_EQ(cameraPixels.getData()[0], static_cast<unsigned char>(3));
    ASSERT_EQ(cameraPixels.getData()[1], static_cast<unsigned char>(2));
    ASSERT_EQ(cameraPixels.getData()[2], static_cast<unsigned char>(1));
    ASSERT_EQ(cameraPixels.getData()[3], static_cast<unsigned char>(4));

    CameraFrameView frameView;
    ASSERT_FALSE(frameView.valid());
    ASSERT_FALSE(camera().latestFrameView(frameView));
    ASSERT_TRUE(camera().availableDevices().empty());

    DeviceInfo device = deviceInfo();
    ASSERT_TRUE(!device.model.empty());

    AudioInterruption interruption;
    interruption.type = AudioInterruptionType::Ended;
    interruption.shouldResume = true;
    ASSERT_EQ(toString(interruption.type), std::string("ended"));

    AudioSessionConfig audioConfig;
    audioConfig.preferredSampleRate = 48000.0;
    audioConfig.preferredIOBufferDuration = 0.005;
    audioConfig.allowBluetoothA2DP = true;
    ASSERT_EQ(audioConfig.preferredSampleRate, 48000.0);

    MotionSample motionSample;
    ASSERT_FALSE(motionSample.hasDeviceMotion);

    LocationConfig locationConfig;
    ASSERT_EQ(toString(locationConfig.accuracy), std::string("hundred meters"));

    NetworkPath networkPath;
    ASSERT_EQ(toString(networkPath.status), std::string("unknown"));

    NotificationSettings notificationSettings;
    notificationSettings.authorizationStatus = PermissionState::Provisional;
    notificationSettings.alert = NotificationSettingState::Enabled;
    ASSERT_EQ(toString(notificationSettings.alert), std::string("enabled"));

    NotificationAction notificationAction;
    notificationAction.identifier = "open";
    notificationAction.title = "Open";
    NotificationCategory notificationCategory;
    notificationCategory.identifier = "project";
    notificationCategory.actions.push_back(notificationAction);
    NotificationResponse notificationResponse;
    notificationResponse.notificationIdentifier = "local";
    notificationResponse.categoryIdentifier = "project";
    ASSERT_EQ(notificationCategory.actions.size(), static_cast<std::size_t>(1));

    BackgroundTaskRequest taskRequest;
    ASSERT_EQ(toString(taskRequest.kind), std::string("app refresh"));
    BackgroundTaskContext taskContext;
    taskContext.identifier = "com.trussc.refresh";
    taskContext.expired = true;
    taskContext.expirationReason = "system expiration";
    ASSERT_TRUE(taskContext.expired);

    BackgroundDownloadRequest downloadRequest;
    downloadRequest.url = "https://example.com/file.dat";
    ASSERT_TRUE(!downloadRequest.sessionIdentifier.empty());
    downloadRequest.persistAcrossRelaunch = true;
    ASSERT_TRUE(downloadRequest.persistAcrossRelaunch);
    ASSERT_TRUE(backgroundDownloads().pendingRequests().empty());

    PickedFile pickedFile;
    pickedFile.localPath = "project.trusscproj";
    pickedFile.contentType = "com.trussc.project";
    pickedFile.copiedIntoSandbox = false;
    pickedFile.securityScopedBookmark = {1, 2, 3};
    ASSERT_EQ(pickedFile.localPath.string(), std::string("project.trusscproj"));
    ASSERT_EQ(pickedFile.contentType, std::string("com.trussc.project"));
    ASSERT_FALSE(pickedFile.copiedIntoSandbox);
    ASSERT_EQ(pickedFile.securityScopedBookmark.size(), static_cast<std::size_t>(3));

    ASSERT_EQ(toString(AppDirectory::Documents), std::string("documents"));

    PhotoPickerRequest photoPickerRequest;
    photoPickerRequest.mediaTypes = PhotoMediaType::ImagesAndVideos;
    ASSERT_EQ(toString(photoPickerRequest.mediaTypes), std::string("images and videos"));
    PickedPhoto pickedPhoto;
    pickedPhoto.filename = "clip.mov";
    pickedPhoto.fileSize = 1024;
    pickedPhoto.durationSeconds = 1.5;
    pickedPhoto.limitedLibrary = true;
    ASSERT_EQ(pickedPhoto.filename, std::string("clip.mov"));
    ASSERT_EQ(pickedPhoto.fileSize, static_cast<std::uint64_t>(1024));
    ASSERT_NEAR(pickedPhoto.durationSeconds, 1.5, 0.0001);
    ASSERT_TRUE(pickedPhoto.limitedLibrary);
    PhotoSaveRequest photoSaveRequest;
    photoSaveRequest.path = "render.png";
    photoSaveRequest.mediaType = PhotoMediaType::Image;
    ASSERT_EQ(toString(photoSaveRequest.mediaType), std::string("image"));

    SceneContext sceneContext;
    sceneContext.identifier = "main";
    sceneContext.active = true;
    scene().upsertContext(sceneContext);
    ASSERT_TRUE(scene().hasActiveScene());
    ASSERT_EQ(scene().contexts().size(), static_cast<std::size_t>(1));
    scene().removeContext("main");

    KeychainItem keychainItem;
    keychainItem.account = "account";
    ASSERT_EQ(toString(keychainItem.accessibility), std::string("when unlocked"));

    AuthenticationRequest authRequest;
    ASSERT_EQ(toString(authRequest.policy), std::string("device owner authentication"));

    SafariRequest safariRequest;
    safariRequest.url = "https://trussc.org";
    ASSERT_TRUE(safariRequest.barCollapsingEnabled);

    ExternalDisplayRequest displayRequest;
    ASSERT_TRUE(!displayRequest.title.empty());

    BLEScanRequest bleScan;
    ASSERT_FALSE(bleScan.allowDuplicates);
    ASSERT_EQ(toString(BluetoothState::PoweredOn), std::string("powered on"));

    MultipeerConfig multipeerConfig;
    ASSERT_TRUE(!multipeerConfig.serviceType.empty());

    GameControllerState gameControllerState;
    ASSERT_FALSE(gameControllerState.connected);

    PencilCanvasRequest pencilRequest;
    ASSERT_TRUE(pencilRequest.showToolPicker);

    StoreProduct product;
    product.identifier = "product";
    ASSERT_EQ(product.identifier, std::string("product"));
    StoreTransactionUpdate transaction;
    transaction.productIdentifier = "product";
    transaction.state = StoreTransactionState::Purchased;
    ASSERT_EQ(toString(transaction.state), std::string("purchased"));

    PickedContact contact;
    contact.givenName = "Ada";
    ASSERT_EQ(contact.givenName, std::string("Ada"));

    ARSessionConfig arConfig;
    ASSERT_TRUE(arConfig.worldTracking);

    VisionRectangle rectangle;
    ASSERT_EQ(rectangle.confidence, 0.0f);

    VisionMaskRequest maskRequest;
    maskRequest.imagePath = "portrait.jpg";
    maskRequest.kind = VisionMaskKind::PersonSegmentation;
    maskRequest.outputWidth = 320;
    maskRequest.outputHeight = 240;
    ASSERT_EQ(maskRequest.imagePath.string(), std::string("portrait.jpg"));
    ASSERT_EQ(maskRequest.kind, VisionMaskKind::PersonSegmentation);
    ASSERT_EQ(maskRequest.outputWidth, 320);

    VisionMaskResult maskResult;
    maskResult.width = 320;
    maskResult.height = 240;
    maskResult.alpha = {0, 128, 255};
    ASSERT_EQ(maskResult.alpha.size(), static_cast<std::size_t>(3));

    CoreMLModelInfo modelInfo;
    ASSERT_FALSE(modelInfo.loadable);

    OperationHandle operation = operations().create("test");
    ASSERT_TRUE(operation.valid());
    ASSERT_FALSE(operation.cancelled());
    operation.cancel();
    ASSERT_TRUE(operation.cancelled());
    operations().remove(operation.identifier());

    OperationHandle fileOperation = files().importFilesCancellable({}, [](Result<std::vector<PickedFile>>) {});
    ASSERT_TRUE(fileOperation.valid());
    fileOperation.cancel();

    OperationHandle photoOperation = photos().pickPhotosCancellable({}, [](Result<std::vector<PickedPhoto>>) {});
    ASSERT_TRUE(photoOperation.valid());
    photoOperation.cancel();

    OperationHandle downloadOperation = backgroundDownloads().downloadCancellable(
        {"https://example.com/file.dat", {}, "download"},
        [](Result<BackgroundDownloadResult>) {});
    ASSERT_TRUE(downloadOperation.valid());
    downloadOperation.cancel();

    OperationHandle bleOperation = bluetoothLE().startScanCancellable({}, [](const BLEPeripheralInfo&) {});
    ASSERT_TRUE(bleOperation.valid());
    bleOperation.cancel();

    LogRecord record;
    record.level = LogLevel::Warning;
    record.message = "message";
    ASSERT_EQ(toString(record.level), std::string("warning"));
}

TEST(configuration_requirements_cover_privacy_and_capabilities) {
    FeatureConfiguration config;
    config.permissions = {
        Permission::Camera,
        Permission::PhotoLibraryAddOnly,
        Permission::Bluetooth
    };
    config.features = {
        IOSFeature::BackgroundTasks,
        IOSFeature::BackgroundDownloads,
        IOSFeature::StoreKit,
        IOSFeature::ARKit,
        IOSFeature::Vision
    };
    config.backgroundTaskIdentifiers = {
        "org.trussc.refresh",
        "org.trussc.processing"
    };

    const AppConfigurationRequirements requirements = configurationRequirementsFor(config);
    const AppConfigurationFragments fragments = configurationFragmentsFor(config);
    ASSERT_TRUE(requirements.infoPlistKeys.size() >= 3);
    ASSERT_TRUE(requirements.backgroundModes.size() >= 2);
    ASSERT_EQ(requirements.backgroundTaskIdentifiers.size(), static_cast<std::size_t>(2));
    ASSERT_TRUE(!requirements.privacyManifestEntries.empty());
    ASSERT_TRUE(!privacyManifestXML(requirements.privacyManifestEntries).empty());
    ASSERT_EQ(fragments.infoPlist.usageDescriptionKeys.size(), requirements.infoPlistKeys.size());
    ASSERT_EQ(fragments.infoPlist.backgroundTaskIdentifiers.size(), static_cast<std::size_t>(2));
    ASSERT_TRUE(fragments.backgroundModes.modes.size() >= 2);
    ASSERT_TRUE(!fragments.entitlements.keys.empty());
    ASSERT_TRUE(!fragments.privacyManifest.entries.empty());
}

TEST(logger_dispatches_records_to_handler) {
    int count = 0;
    LogRecord captured;
    logger().setHandler([&](const LogRecord& record) {
        ++count;
        captured = record;
    });

    logger().warning("tcxIOS", "route changed");
    logger().clearHandler();

    ASSERT_EQ(count, 1);
    ASSERT_EQ(captured.level, LogLevel::Warning);
    ASSERT_EQ(captured.subsystem, std::string("tcxIOS"));
    ASSERT_EQ(captured.message, std::string("route changed"));

    count = 0;
    Error nativeError;
    nativeError.code = ErrorCode::NativeError;
    nativeError.message = "native message";
    nativeError.nativeCode = -1009;
    nativeError.nativeDomain = "NSURLErrorDomain";
    logger().setHandler([&](const LogRecord& record) {
        ++count;
        captured = record;
    });

    logger().error("tcxIOS.native", "network failed", nativeError);
    logger().clearHandler();

    ASSERT_EQ(count, 1);
    ASSERT_EQ(captured.error.nativeDomain, std::string("NSURLErrorDomain"));
    ASSERT_EQ(captured.nativeDomain, std::string("NSURLErrorDomain"));
    ASSERT_EQ(captured.nativeCode, -1009);
}

TEST(public_headers_have_no_native_imports) {
    const std::filesystem::path includeDir = std::filesystem::path(TCX_IOS_TEST_ADDON_DIR) / "include";
    const std::vector<std::string> banned = {
        "#import",
        "UIKit",
        "Foundation",
        "AVFoundation",
        "PhotosUI",
        "UIViewController",
        "UIImage",
        "NSURL",
        "NSString",
        "CVPixelBufferRef"
    };

    int checked = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(includeDir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".h") continue;
        if (entry.path().string().find("/native/") != std::string::npos) continue;
        std::ifstream in(entry.path());
        std::stringstream buffer;
        buffer << in.rdbuf();
        const std::string text = buffer.str();
        for (const auto& token : banned) {
            if (text.find(token) != std::string::npos) {
                throw std::runtime_error(entry.path().string() + " contains native token: " + token);
            }
        }
        ++checked;
    }

    ASSERT_TRUE(checked >= 24);
}
