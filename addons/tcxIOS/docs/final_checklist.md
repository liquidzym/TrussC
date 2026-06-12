# tcxIOS final validation checklist

This checklist is intentionally device-focused. The 2026-06-06 local pass covers
public headers, macOS stubs, project generation, and unsigned `iphoneos` builds;
it does not include physical device installation or runtime validation.

## Real device

- Camera: permission prompt, front/back selection, frame ids, dropped frame
  counter, `latestFrameView()` lifetime, texture upload path, orientation, and
  mirroring.
- Photos and files: PHPicker image/video import, Photos image/video save,
  document copy and open-in-place access, `Files::stopAccessing()`.
- Audio and sensors: microphone prompt, audio route override, Bluetooth HFP/A2DP
  routing, haptics, CoreMotion sampling.
- Notifications: authorization prompt, categories/actions, delivered
  notification tap/action callbacks.
- Location and network: when-in-use prompt, location updates, NWPathMonitor
  state changes.
- BackgroundTasks: permitted identifier configuration, expiration handler, app
  refresh and processing scheduling behavior.
- Background downloads: background mode configuration, persistent request
  registry, cancel path, foreground completion, AppDelegate
  `TCXIOSHandleBackgroundURLSessionEvents()` handoff, and relaunch rebind
  behavior.
- BLE and Multipeer: scan/connect/read/write/notify, peer discovery, invitation,
  reliable message send/receive.
- StoreKit: product lookup, sandbox purchase, restore, transaction update
  handler, cancellation/error paths.
- iPad-specific: PencilKit drawing/capture, external display presentation,
  multi-scene presenter registration.
- AR/Vision/CoreML: AR world tracking support, session start/stop, frame query,
  Vision rectangle detection, foreground-instance mask on iOS 17+, person
  segmentation mask on iOS 15+, mask output resizing, and compiled CoreML model
  inspection.

## Preflight commands

```bash
cmake --build tests/build-macos --parallel 4
tests/build-macos/tcxIOS_tests
trusscli update -p examples/ios-basic-shell --ios
xcodebuild -project examples/ios-basic-shell/xcode-ios/ios-basic-shell.xcodeproj \
  -scheme ios-basic-shell -configuration Debug -sdk iphoneos \
  CODE_SIGNING_ALLOWED=NO build
```

Run the same `trusscli update --ios` and unsigned `iphoneos` build for every
example before installing on a physical device.
