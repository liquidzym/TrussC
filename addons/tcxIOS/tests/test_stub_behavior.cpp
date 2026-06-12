#include "test_common.h"

#include "tcxIOS.h"

using namespace tcx::ios;

namespace {

template <typename T, typename Fn>
Result<T> captureAsync(Fn&& fn) {
    eventQueue().clear();
    bool called = false;
    Result<T> result;
    fn([&](Result<T> r) {
        called = true;
        result = std::move(r);
    });
    ASSERT_FALSE(called);
    ASSERT_TRUE(eventQueue().pending() > 0);
    update();
    ASSERT_TRUE(called);
    return result;
}

} // namespace

TEST(stub_app_device_defaults) {
    ASSERT_EQ(app().state(), AppState::Active);
    ASSERT_EQ(app().orientation(), Orientation::Unknown);

    DeviceInfo info = app().deviceInfo();
    ASSERT_EQ(info.model, std::string("non-ios"));
    ASSERT_EQ(info.systemName, std::string("Non-iOS stub"));

    ScreenInfo screen = app().mainScreen();
    ASSERT_EQ(screen.pixelWidth, 0);
    ASSERT_EQ(screen.pixelHeight, 0);
}

TEST(stub_async_operations_return_unavailable) {
    auto alert = captureAsync<AlertResult>([](auto done) {
        nativeUI().showAlert({"Title", "Message", {"OK"}, -1}, done);
    });
    ASSERT_FALSE(alert.ok);
    ASSERT_EQ(alert.error.code, ErrorCode::Unavailable);

    auto openURL = captureAsync<void>([](auto done) {
        nativeUI().openURL("https://trussc.org", done);
    });
    ASSERT_FALSE(openURL.ok);
    ASSERT_EQ(openURL.error.code, ErrorCode::Unavailable);

    auto permission = captureAsync<PermissionState>([](auto done) {
        permissions().request(Permission::Camera, done);
    });
    ASSERT_FALSE(permission.ok);
    ASSERT_EQ(permission.error.code, ErrorCode::Unavailable);

    auto imported = captureAsync<std::vector<PickedFile>>([](auto done) {
        files().importFiles({}, done);
    });
    ASSERT_FALSE(imported.ok);
    ASSERT_EQ(imported.error.code, ErrorCode::Unavailable);

    auto exported = captureAsync<void>([](auto done) {
        files().exportFile({}, done);
    });
    ASSERT_FALSE(exported.ok);
    ASSERT_EQ(exported.error.code, ErrorCode::Unavailable);

    auto picked = captureAsync<std::vector<PickedPhoto>>([](auto done) {
        photos().pickPhotos({}, done);
    });
    ASSERT_FALSE(picked.ok);
    ASSERT_EQ(picked.error.code, ErrorCode::Unavailable);

    auto savedPhoto = captureAsync<void>([](auto done) {
        photos().save({"render.png", PhotoMediaType::Image}, done);
    });
    ASSERT_FALSE(savedPhoto.ok);
    ASSERT_EQ(savedPhoto.error.code, ErrorCode::Unavailable);

    auto cameraStart = captureAsync<void>([](auto done) {
        camera().start({}, done);
    });
    ASSERT_FALSE(cameraStart.ok);
    ASSERT_EQ(cameraStart.error.code, ErrorCode::Unavailable);

    auto audioCategory = captureAsync<void>([](auto done) {
        audioSession().setCategory({}, done);
    });
    ASSERT_FALSE(audioCategory.ok);
    ASSERT_EQ(audioCategory.error.code, ErrorCode::Unavailable);

    auto audioSpeaker = captureAsync<void>([](auto done) {
        audioSession().overrideOutputToSpeaker(true, done);
    });
    ASSERT_FALSE(audioSpeaker.ok);
    ASSERT_EQ(audioSpeaker.error.code, ErrorCode::Unavailable);

    auto motionStart = captureAsync<void>([](auto done) {
        motion().start({}, done);
    });
    ASSERT_FALSE(motionStart.ok);
    ASSERT_EQ(motionStart.error.code, ErrorCode::Unavailable);

    auto notification = captureAsync<std::string>([](auto done) {
        notifications().schedule({"id", "Title", "Body", 1.0, false}, done);
    });
    ASSERT_FALSE(notification.ok);
    ASSERT_EQ(notification.error.code, ErrorCode::Unavailable);

    auto notificationSettings = captureAsync<NotificationSettings>([](auto done) {
        notifications().settings(done);
    });
    ASSERT_FALSE(notificationSettings.ok);
    ASSERT_EQ(notificationSettings.error.code, ErrorCode::Unavailable);

    auto notificationCategories = captureAsync<void>([](auto done) {
        notifications().setCategories({{"project", {{"open", "Open", false, false}}, false}}, done);
    });
    ASSERT_FALSE(notificationCategories.ok);
    ASSERT_EQ(notificationCategories.error.code, ErrorCode::Unavailable);

    auto locationPermission = captureAsync<PermissionState>([](auto done) {
        location().requestWhenInUse(done);
    });
    ASSERT_FALSE(locationPermission.ok);
    ASSERT_EQ(locationPermission.error.code, ErrorCode::Unavailable);

    auto backgroundTask = captureAsync<void>([](auto done) {
        backgroundTasks().registerHandler({"com.trussc.test.refresh", BackgroundTaskKind::AppRefresh},
                                          [](const BackgroundTaskContext&) { return true; },
                                          done);
    });
    ASSERT_FALSE(backgroundTask.ok);
    ASSERT_EQ(backgroundTask.error.code, ErrorCode::Unavailable);

    auto backgroundDownload = captureAsync<BackgroundDownloadResult>([](auto done) {
        backgroundDownloads().download({"https://example.com/file.dat", {}, "test"}, done);
    });
    ASSERT_FALSE(backgroundDownload.ok);
    ASSERT_EQ(backgroundDownload.error.code, ErrorCode::Unavailable);

    auto auth = captureAsync<AuthenticationResult>([](auto done) {
        localAuthentication().evaluate({"Authenticate", AuthenticationPolicy::DeviceOwnerAuthentication}, done);
    });
    ASSERT_FALSE(auth.ok);
    ASSERT_EQ(auth.error.code, ErrorCode::Unavailable);

    auto safari = captureAsync<void>([](auto done) {
        web().openSafari({"https://trussc.org"}, done);
    });
    ASSERT_FALSE(safari.ok);
    ASSERT_EQ(safari.error.code, ErrorCode::Unavailable);

    auto display = captureAsync<ExternalDisplayPresentation>([](auto done) {
        externalDisplay().show({}, done);
    });
    ASSERT_FALSE(display.ok);
    ASSERT_EQ(display.error.code, ErrorCode::Unavailable);

    auto bleConnect = captureAsync<void>([](auto done) {
        bluetoothLE().connect("peripheral", done);
    });
    ASSERT_FALSE(bleConnect.ok);
    ASSERT_EQ(bleConnect.error.code, ErrorCode::Unavailable);

    auto bleRead = captureAsync<BLECharacteristicValue>([](auto done) {
        bluetoothLE().read({"peripheral", "service", "characteristic"}, done);
    });
    ASSERT_FALSE(bleRead.ok);
    ASSERT_EQ(bleRead.error.code, ErrorCode::Unavailable);

    auto bleWrite = captureAsync<void>([](auto done) {
        bluetoothLE().write({{"peripheral", "service", "characteristic"}, {1, 2, 3}, true}, done);
    });
    ASSERT_FALSE(bleWrite.ok);
    ASSERT_EQ(bleWrite.error.code, ErrorCode::Unavailable);

    auto bleNotify = captureAsync<BLECharacteristicValue>([](auto done) {
        bluetoothLE().setNotify({"peripheral", "service", "characteristic"}, true, done);
    });
    ASSERT_FALSE(bleNotify.ok);
    ASSERT_EQ(bleNotify.error.code, ErrorCode::Unavailable);

    auto multipeerStart = captureAsync<void>([](auto done) {
        multipeer().start({}, done);
    });
    ASSERT_FALSE(multipeerStart.ok);
    ASSERT_EQ(multipeerStart.error.code, ErrorCode::Unavailable);

    auto multipeerSend = captureAsync<void>([](auto done) {
        multipeer().send({1, 2, 3}, done);
    });
    ASSERT_FALSE(multipeerSend.ok);
    ASSERT_EQ(multipeerSend.error.code, ErrorCode::Unavailable);

    auto pencilPresent = captureAsync<void>([](auto done) {
        pencilCanvas().present({}, done);
    });
    ASSERT_FALSE(pencilPresent.ok);
    ASSERT_EQ(pencilPresent.error.code, ErrorCode::Unavailable);

    auto products = captureAsync<std::vector<StoreProduct>>([](auto done) {
        storeKit().requestProducts({"product"}, done);
    });
    ASSERT_FALSE(products.ok);
    ASSERT_EQ(products.error.code, ErrorCode::Unavailable);

    auto purchase = captureAsync<StorePurchaseResult>([](auto done) {
        storeKit().purchase("product", done);
    });
    ASSERT_FALSE(purchase.ok);
    ASSERT_EQ(purchase.error.code, ErrorCode::Unavailable);

    auto contact = captureAsync<PickedContact>([](auto done) {
        contactsUI().pickContact(done);
    });
    ASSERT_FALSE(contact.ok);
    ASSERT_EQ(contact.error.code, ErrorCode::Unavailable);

    auto arStart = captureAsync<void>([](auto done) {
        arKit().start({}, done);
    });
    ASSERT_FALSE(arStart.ok);
    ASSERT_EQ(arStart.error.code, ErrorCode::Unavailable);

    auto rectangles = captureAsync<std::vector<VisionRectangle>>([](auto done) {
        vision().detectRectangles({}, done);
    });
    ASSERT_FALSE(rectangles.ok);
    ASSERT_EQ(rectangles.error.code, ErrorCode::Unavailable);

    auto mask = captureAsync<VisionMaskResult>([](auto done) {
        vision().makeMask({"portrait.jpg", VisionMaskKind::ForegroundInstances, 64, 64}, done);
    });
    ASSERT_FALSE(mask.ok);
    ASSERT_EQ(mask.error.code, ErrorCode::Unavailable);
}

TEST(stub_sync_features_return_false) {
    const std::vector<Permission> permissionsToCheck = {
        Permission::Camera,
        Permission::Microphone,
        Permission::PhotoLibraryRead,
        Permission::PhotoLibraryAddOnly,
        Permission::LocationWhenInUse,
        Permission::LocationAlways,
        Permission::Notifications,
        Permission::Bluetooth,
        Permission::Motion,
        Permission::Contacts
    };
    for (Permission permission : permissionsToCheck) {
        ASSERT_EQ(permissions().status(permission), PermissionState::Unknown);
    }

    ASSERT_FALSE(camera().isRunning());
    ASSERT_FALSE(motion().isRunning());
    ASSERT_FALSE(haptics().impact());
    ASSERT_FALSE(haptics().selection());
    ASSERT_FALSE(haptics().notification(HapticNotificationType::Success));

    CameraFrame frame;
    ASSERT_FALSE(camera().latestFrame(frame));

    MotionSample sample;
    ASSERT_FALSE(motion().latest(sample));

    AudioRoute route = audioSession().currentRoute();
    ASSERT_TRUE(route.inputs.empty());
    ASSERT_TRUE(route.outputs.empty());

    LocationSample locationSample;
    ASSERT_EQ(location().authorizationStatus(), PermissionState::Unknown);
    ASSERT_FALSE(location().isRunning());
    ASSERT_FALSE(location().latest(locationSample));

    ASSERT_TRUE(!files().directoryPath(AppDirectory::Documents).empty());
    ASSERT_TRUE(!files().documentsDirectory().empty());
    ASSERT_TRUE(!files().cachesDirectory().empty());
    ASSERT_TRUE(!files().temporaryDirectory().empty());
    ASSERT_TRUE(!files().applicationSupportDirectory().empty());

    ASSERT_FALSE(networkStatus().isRunning());
    ASSERT_EQ(networkStatus().current().status, NetworkPathStatus::Unknown);

    ASSERT_FALSE(keychain().setString("service", "account", "value").ok);
    ASSERT_FALSE(keychain().get("service", "account").ok);
    ASSERT_FALSE(keychain().remove("service", "account").ok);
    ASSERT_FALSE(localAuthentication().availability(AuthenticationPolicy::DeviceOwnerAuthentication).available);
    ASSERT_TRUE(externalDisplay().screens().empty());
    ASSERT_FALSE(externalDisplay().hasExternalScreen());

    ASSERT_EQ(bluetoothLE().state(), BluetoothState::Unsupported);
    ASSERT_TRUE(multipeer().peers().empty());
    ASSERT_TRUE(gameController().connectedControllerNames().empty());

    GameControllerState gamepad;
    ASSERT_FALSE(gameController().latest(gamepad));

    ASSERT_FALSE(pencilCanvas().capture().ok);
    ASSERT_FALSE(storeKit().canMakePayments());
    ASSERT_FALSE(arKit().isWorldTrackingSupported());

    ARFrameInfo arFrame;
    ASSERT_FALSE(arKit().latestFrame(arFrame));
    ASSERT_FALSE(coreML().inspectModel({}).ok);
}
