# tcxIOS

`tcxIOS` is a TrussC-friendly iOS and iPadOS system capability layer. It keeps
UIKit, Foundation, AVFoundation, PhotosUI, CoreMotion, CoreBluetooth,
MultipeerConnectivity, PencilKit, StoreKit, ARKit, Vision, CoreML, and related
system framework details behind a C++ facade so app code can stay in TrussC.

This v0.3 implementation provides:

- Public include: `#include "tcxIOS.h"`
- App, scene registry, lifecycle callback, screen, safe-area, orientation, and
  device query interfaces
- Central permission status/request facade
- `Operations` cancellation handles and `Logger` records for native/system
  diagnostics
- Native UI facade for alerts, share sheet, settings, and URL opening
- File picker import/export, PHPicker image/video import, Photos image/video
  save, camera frame capture, audio session, haptics, motion, and local
  notification facades
- App sandbox directory helpers for Documents, Caches, Temporary, and
  Application Support
- Location, NetworkStatus/NWPathMonitor, BackgroundTasks, background URLSession,
  Keychain, LocalAuthentication, Web/Safari, and ExternalDisplay facades
- Bluetooth LE, MultipeerConnectivity, GameController, PencilKit, StoreKit,
  ContactsUI, ARKit, Vision, and CoreML bridge facades
- Feature configuration helpers for Info.plist keys, background modes,
  entitlements, BGTask identifiers, and PrivacyInfo fragments
- Native bridge split into focused Objective-C++ modules under `src/ios/`
- Non-iOS stub implementation with explicit unavailable errors
- iOS bridge coverage for app/device info, lifecycle notifications,
  alert/share/openURL, selected permissions, document picker, PHPicker image
  and video selection, Photos save, camera BGRA frames, camera device/format
  enumeration, no-copy frame views, audio session, haptics, motion, local
  notifications with categories/actions/response callbacks, background task
  expiration handling, persistent background download request metadata, and
  StoreKit restore/update streams
- Tests for public headers, event queue behavior, feature manifest metadata,
  logger/operation behavior, and non-iOS stub results
- v0.1 examples:
  - `examples/ios-basic-shell`
  - `examples/ios-files-photos-share`
  - `examples/ios-camera-texture`
- v0.2 examples:
  - `examples/ios-location-notification`
  - `examples/ios-background-download`
  - `examples/ios-security-external-display`
- v0.3 examples:
  - `examples/ios-ble-multipeer`
  - `examples/ios-game-pencil-store-contact`
  - `examples/ios-ar-vision-coreml`

## Include

```cpp
#include "tcxIOS.h"

void update() {
    tcx::ios::update();
}
```

Native callbacks are routed through `tcx::ios::eventQueue()`. Drain it from the
TrussC update thread with `tcx::ios::update()` before touching app state or GPU
resources. Camera capture callbacks copy BGRA frames into CPU memory only;
`Camera::copyLatestFrameToPixels()` and `Camera::uploadLatestFrameToTexture()`
must be called from the TrussC update/render side.

## Scene and App Lifecycle

`tcx::ios::scene()` keeps a lightweight registry of active scene identifiers and
safe-area values. The iOS bridge observes app state, orientation, and screen
connect/disconnect notifications, then posts `App` callbacks through
`eventQueue()`. Host shells that own a UIKit view controller can register it
with the native `TCXIOSBridge` scene selectors so modal presentation can prefer
the active scene before falling back to the foreground key window.

## Info.plist

Apps that enable native iOS features must provide purpose strings. The helper
`tcx::ios::defaultInfoPlistUsageDescriptions()` returns the recommended keys:

- `NSCameraUsageDescription`
- `NSMicrophoneUsageDescription`
- `NSPhotoLibraryUsageDescription`
- `NSPhotoLibraryAddUsageDescription`
- `NSMotionUsageDescription`
- `NSLocationWhenInUseUsageDescription`
- `NSLocationAlwaysAndWhenInUseUsageDescription`
- `NSBluetoothAlwaysUsageDescription`
- `NSContactsUsageDescription`

Camera, microphone, photo library, location, Bluetooth, contacts, and other
sensitive features should not request permission until the app has a matching
purpose string.

Local notifications do not use an Info.plist usage-description key. Use
`Permissions::request(Permission::Notifications)` to trigger
`UNUserNotificationCenter` authorization, then handle denied/restricted states
in app UI.

`Permissions::status()` covers Camera, Microphone, PhotoLibrary, Location,
Notifications, Bluetooth, Motion, and Contacts on iOS. Notification status is
cached from the last request/settings query because Apple's exact notification
settings API is asynchronous. Use `notifications().settings()` when the app
needs current notification authorization and per-channel settings.

For app target setup, `FeatureConfiguration` and
`configurationRequirementsFor()` return the Info.plist keys, background modes,
entitlements, BGTask identifiers, and privacy manifest entries needed by a
feature set. `scripts/tcxios_manifest_tool.py` exposes the same workflow as a
command-line helper and can generate Info.plist / `PrivacyInfo.xcprivacy`
fragments or check existing app plists.

## Notifications

`tcx::ios::notifications().settings()` returns current authorization plus alert,
sound, badge, notification center, lock screen, CarPlay, critical alert, and time
sensitive settings. `setCategories()` registers action categories,
`LocalNotificationRequest::categoryIdentifier` schedules actionable
notifications, and `setResponseHandler()` receives tap/action responses on the
TrussC event queue.

## Photos

`tcx::ios::photos()` can pick images, videos, or both through PHPicker and report
the selected media type on each `PickedPhoto`. `Photos::save()` writes image or
video files into the user's Photos library through PhotoKit. As with all Photos
writes, apps must provide `NSPhotoLibraryAddUsageDescription` and handle denied
or limited authorization.

## Files

`tcx::ios::files()` exposes app sandbox paths:

- `documentsDirectory()`
- `cachesDirectory()`
- `temporaryDirectory()`
- `applicationSupportDirectory()`

`ImportFileRequest::copyIntoApp` controls document picker behavior. When it is
`true`, selected files are copied into a temporary app sandbox folder. When it is
`false`, the picker uses open-in-place mode and `PickedFile` reports
`copiedIntoSandbox = false`, the original `localPath`, `contentType`, and
`securityScoped = true` when the URL needs scoped access. Call
`Files::stopAccessing()` when finished. iOS does not expose the macOS
security-scoped bookmark creation option, so `securityScopedBookmark` is
reserved for future compatible platforms and remains empty on iOS. Existing
`path`, `uti`, and `securityScoped` fields are kept as compatibility aliases.

## Camera

The current camera path is CPU BGRA copy first. Each `CameraFrame` carries
`frameId` and `droppedFrameCount` so apps can detect stalled consumers or late
frame drops. `CameraConfig` supports front/back/external device preference,
orientation, mirroring, and ring-buffer capacity. `Camera::availableDevices()`
reports native devices and available format/fps ranges, and
`Camera::latestFrameView()` exposes the latest frame without an additional
`std::vector` copy while keeping the backing storage alive. Direct
`CVPixelBuffer`/Metal texture bridging remains future work.

## AudioSession

`AudioSessionConfig` supports preferred sample rate, preferred IO buffer
duration, default-to-speaker, Bluetooth HFP, and Bluetooth A2DP routing options.
`AudioSession::overrideOutputToSpeaker()` exposes the runtime speaker override
for play-and-record style sessions.

Some v0.2/v0.3 capabilities also require capability-specific app configuration:

- `BGTaskSchedulerPermittedIdentifiers` for BackgroundTasks
- StoreKit product identifiers and sandbox/App Store configuration for purchases
- ARKit-capable hardware for AR world tracking
- An iPad and attached display for ExternalDisplay validation

## PrivacyInfo.xcprivacy

The addon provides `privacyManifestXML()` and
`scripts/tcxios_manifest_tool.py --output-privacy` to generate a
`PrivacyInfo.xcprivacy` fragment for enabled tcxIOS features. Apps that ship to
TestFlight or the App Store still need to merge that fragment with the app's own
privacy answers and review the final manifest for the product's actual data
collection behavior.

## Simulator vs Real Device

Simulator can cover:

- App lifecycle and screen/safe-area queries
- Alerts
- Document picker UI
- Some PHPicker, document picker, and share-sheet flows
- Some local notification flows

Real iPhone or iPad is still required for final validation of:

- Camera frames
- Microphone and audio route behavior
- Haptics
- Motion sensors
- Bluetooth
- Multipeer discovery with nearby peers
- External display
- Background behavior
- StoreKit purchase flows
- ARKit camera/session behavior
- Pencil input
- Game controller input

The current repository verification intentionally does not include physical
device installation or runtime testing.

## Build

For examples:

```bash
trusscli update -p examples/ios-basic-shell --ios
xcodebuild -project examples/ios-basic-shell/xcode-ios/ios-basic-shell.xcodeproj \
  -scheme ios-basic-shell -configuration Debug -sdk iphoneos \
  CODE_SIGNING_ALLOWED=NO build
```

The standalone addon tests exercise public headers and non-iOS stubs, but the
primary validation for this addon is iOS project generation plus unsigned
`iphoneos` builds of the examples.
