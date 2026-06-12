# Reshot iOS tcxIOS Taskbook

Source taskbook: `/Users/mac/Desktop/VIBECODING/TrussC/VIBESHOT/trussc_openreshot_ios_taskbook.md`

Goal: extend `tcxIOS` into the native iOS capability layer needed by a TrussC
photo reshot app: pick one image, prepare it for an on-device Core ML model,
render an interactive reconstructed view, snapshot the selected pose, then save
or share the result.

## Scope Decision

The OpenReshot reference is a single-photo reconstruction workflow, not a live
camera or video recorder workflow. The first tcxIOS phase should prioritize
native image/model/rendering infrastructure:

- Core ML compile/load/predict runtime.
- Model package download and install management.
- Image metadata and tensor preprocessing.
- Metal surface or native view bridge for interactive preview and snapshot.
- Vision subject/person mask generation.
- Optional generic HTTP client for product-layer enhancement services.

`Camera` and video recording remain second-phase features unless the product
adds direct capture or interaction-recording requirements.

## Current Iteration: Vision Masks

Implemented in this pass:

- `VisionMaskKind`, `VisionMaskRequest`, and `VisionMaskResult`.
- `VisionBridge::makeMask()`.
- iOS foreground-instance masks through Vision on iOS 17 or newer.
- iOS person-segmentation masks through Vision on iOS 15 or newer.
- Non-iOS stub behavior returning `ErrorCode::Unavailable`.
- `IOSFeature::Vision` so manifest tooling can accept projects that explicitly
  name Vision while adding no extra Info.plist key, entitlement, or background
  mode.
- Example shortcut in `examples/ios-ar-vision-coreml`.

Validation target:

```bash
cmake --build tests/build-macos
tests/build-macos/tcxIOS_tests
trusscli update -p examples/ios-ar-vision-coreml --ios
xcodebuild -project examples/ios-ar-vision-coreml/xcode-ios/ios-ar-vision-coreml.xcodeproj \
  -scheme ios-ar-vision-coreml -configuration Debug -sdk iphoneos \
  CODE_SIGNING_ALLOWED=NO build
```

## Next Slice 1: Core ML Runtime

Files:

- Create `include/tcx/ios/CoreMLRuntime.h`.
- Create `src/ios/TCXIOSCoreMLRuntime.mm`.
- Modify `include/tcxIOS.h`.
- Modify `src/common/Services.cpp`.
- Modify `src/private/TCXIOSPlatform.h`.
- Add tests for tensor shape/type storage, unavailable stub responses, and
  public-header native import boundaries.

Acceptance:

- Compile `.mlpackage` to `.mlmodelc`.
- Load a compiled model with selected compute units.
- Run async prediction using typed C++ tensor inputs and outputs.
- Return through `EventQueue`.
- Expose no Objective-C Core ML types in public C++ headers.

## Next Slice 2: Model Assets

Files:

- Create `include/tcx/ios/ModelAssets.h`.
- Create `src/ios/TCXIOSModelAssets.mm`.
- Modify `include/tcxIOS.h`.
- Modify `src/common/Services.cpp`.
- Modify `src/private/TCXIOSPlatform.h`.
- Extend docs with SHARP `.mlpackage` bundle and on-demand install flows.

Acceptance:

- Detect bundle and downloaded compiled models.
- Download package files into staging.
- Support progress and cancellation.
- Compile after download when requested.
- Atomically install or remove downloaded assets under Application Support.

## Next Slice 3: Image IO Bridge

Files:

- Create `include/tcx/ios/ImageIOBridge.h`.
- Create `src/ios/TCXIOSImageIOBridge.mm`.
- Modify `include/tcxIOS.h`.
- Modify `src/common/Services.cpp`.
- Modify `src/private/TCXIOSPlatform.h`.

Acceptance:

- Read JPEG, HEIC, and PNG metadata.
- Normalize orientation.
- Read focal length metadata when available.
- Produce RGB Float32 CHW tensors sized for model input.
- Perform work off the main thread and complete through `EventQueue`.

## Next Slice 4: Metal Surface

Files:

- Create `include/tcx/ios/MetalSurface.h`.
- Create `include/tcx/ios/native/TCXIOSMetalSurfaceObjC.h`.
- Create `src/ios/TCXIOSMetalSurface.mm`.
- Modify `include/tcxIOS.h`.
- Modify `src/common/Services.cpp`.
- Modify `src/private/TCXIOSPlatform.h`.

Acceptance:

- Create and destroy named `MTKView` surfaces.
- Report pixel size and scale.
- Support snapshot to an image file.
- Keep UIKit and Metal object types out of the non-native public C++ API.
- Work with registered scene presenters.

## App-Layer Boundary

Keep Gaussian cloud construction, splat sorting, SHARP-specific tensor naming,
Gemini/image-enhancement prompts, and Reshot UI composition outside tcxIOS. They
belong in the TrussC app or a separate Reshot-specific module. tcxIOS should
provide reusable iOS capabilities with clear C++ facades and predictable
unavailable errors on non-iOS platforms.
