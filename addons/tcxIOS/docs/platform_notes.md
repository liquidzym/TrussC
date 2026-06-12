# tcxIOS platform notes

`tcxIOS` keeps native iOS framework use behind the C++ facade, but the host app
is still responsible for app-level configuration, signing, and final runtime
validation.

## App configuration

Use `scripts/tcxios_manifest_tool.py` to derive required app configuration from
the permissions and feature groups that a project enables:

```bash
scripts/tcxios_manifest_tool.py \
  --permissions camera,photo-library-read,photo-library-add,location-when-in-use \
  --features camera,photo-library,background-tasks \
  --background-task-id com.example.refresh \
  --output-info-plist build/tcxios-info-fragment.plist \
  --output-background-modes build/tcxios-background-modes.plist \
  --output-entitlements build/tcxios-entitlements.plist \
  --output-privacy build/PrivacyInfo.xcprivacy \
  --output-requirements build/tcxios-requirements.json
```

The generated files are fragments. Merge them into the app target's real
`Info.plist`, entitlements, and privacy manifest according to the app's product
requirements and App Store privacy answers.

## Native bridge layout

The Objective-C++ bridge is split by ownership:

- `TCXIOSBridge.mm`: app lifecycle wiring, native UI, app/device/screen queries.
- `TCXIOSBridgeSupport.*`: private shared bridge helpers, scene presenter
  lookup, native error mapping, and common ObjC++ conversion helpers.
- `include/tcx/ios/native/TCXIOSBridgeObjC.h`: host integration hooks for scene
  presenter registration and background URLSession relaunch handoff.
- `TCXIOSFilesPhotos.mm`: document picker, app directories, PHPicker, Photos
  save.
- `TCXIOSCamera.mm`: AVFoundation camera capture, device/formats, frame view.
- `TCXIOSAudioMotionHaptics.mm`: AVAudioSession, haptics, CoreMotion.
- `TCXIOSLocationNotificationBackground.mm`: permissions, location,
  notifications, Network.framework, BackgroundTasks, background downloads.
- `TCXIOSSecurityWebExternal.mm`: Keychain, LocalAuthentication, Safari, external
  display.
- v0.3 feature modules: Bluetooth, Multipeer, GameController, PencilKit,
  StoreKit, Contacts, AR/Vision/CoreML.

## Known platform boundaries

- Camera currently provides CPU BGRA frames, ring-buffer lifetime protection,
  device format enumeration, no-copy `CameraFrameView`, and BGRA-to-RGBA helper
  conversion for TrussC `Pixels`/`Texture`. Direct `CVPixelBuffer`/Metal texture
  bridging is still future work.
- Background download persistence records request metadata. Host apps must call
  `TCXIOSHandleBackgroundURLSessionEvents()` from the AppDelegate background
  URLSession callback to rebind the delegate after relaunch. The system
  completion handler is retained and called after pending URLSession events
  finish; C++ progress/completion callbacks still need app-level rebinding if the
  process was relaunched.
- `BackgroundTaskHandler` runs from the system background task launch path. Treat
  it as background-safe work only; do not perform render-thread or UIKit modal
  work from that handler.
- StoreKit uses a StoreKit 1 compatibility path with product lookup, purchase,
  restore, and transaction updates. StoreKit 2 subscription status and signed
  transaction verification are still future work.
- ExternalDisplay uses a compatibility `UIWindow.screen` presentation path; an
  iPad scene-based external render surface still requires product integration.
- Vision subject masks are returned as C++ alpha buffers. Foreground instance
  masks use `VNGenerateForegroundInstanceMaskRequest` and require iOS 17 or
  newer. Person segmentation masks use `VNGeneratePersonSegmentationRequest`
  and require iOS 15 or newer. Neither feature adds its own Info.plist key, but
  apps still need the appropriate Photos permission strings when the image came
  from the user's library.
