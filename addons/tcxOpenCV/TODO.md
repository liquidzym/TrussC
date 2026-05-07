# tcxOpenCV - Development Plan

## Overview

TrussC addon for OpenCV integration.
This is a **separate repository** due to OpenCV's license complexity (BSD/Apache 2.0 core, but some contrib modules have different licenses).

## Design Decisions

### Naming
- Folder: `tcxOpenCV` (follows TrussC addon convention)
- Repository: `tcxOpenCV` (independent repo, users clone into `addons/`)

### Namespace Strategy

**Option A: Minimal glue (recommended)**
```cpp
#include <tcxOpenCV.h>
using namespace tc;

// Conversion utilities with clear naming (no collision with other addons)
cv::Mat mat = tcx::toCvMat(image);        // tc::Image -> cv::Mat
tc::Image img = tcx::toTcImage(mat);      // cv::Mat -> tc::Image
cv::Mat cam = tcx::toCvMat(grabber);      // tc::VideoGrabber -> cv::Mat

// Use OpenCV as-is
cv::cvtColor(mat, mat, cv::COLOR_BGR2GRAY);
cv::GaussianBlur(mat, mat, cv::Size(5, 5), 0);
```

**Option B: Full wrapper**
```cpp
// Wrap everything in tcx::cv::
tcx::cv::Image img;
img.load("photo.jpg");
img.blur(5);
img.toGray();
```

**Decision: Option A (Minimal glue)**

Reasons:
- OpenCV has extensive documentation and community resources
- Users familiar with OpenCV can use their existing knowledge
- Wrapping all of OpenCV is impractical and unmaintainable
- TrussC philosophy: thin wrappers, not reinventing the wheel

### What we provide

1. **Type conversion utilities** (essential)
   - `tc::Image` <-> `cv::Mat`
   - `tc::Color` <-> `cv::Scalar`
   - `tc::Vec2` <-> `cv::Point2f`
   - `tc::Rect` <-> `cv::Rect`

2. **Convenience wrappers** (optional, commonly used features)
   - Contour detection with tc types
   - Face/object detection helpers
   - Optical flow visualization

3. **TrussC integration**
   - Draw cv::Mat directly with `drawImage()`
   - Camera capture that returns tc::Image (or just use cv::VideoCapture directly?)

## OpenCV Acquisition

**Method: FetchContent**

```cmake
include(FetchContent)
FetchContent_Declare(
    opencv
    GIT_REPOSITORY https://github.com/opencv/opencv.git
    GIT_TAG 4.9.0
)

# Minimal build - disable unnecessary modules
set(BUILD_LIST "core,imgproc,imgcodecs,videoio,objdetect,features2d" CACHE STRING "" FORCE)
set(BUILD_opencv_apps OFF CACHE BOOL "" FORCE)
set(BUILD_opencv_python3 OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_PERF_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_DOCS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(opencv)
```

Pros:
- Self-contained, no system dependencies
- Consistent version across all users
- Works on all platforms

Cons:
- Long initial build time (~10-20 min)
- Large download

Alternative: `find_package(OpenCV)` for users who prefer system OpenCV.

## Directory Structure

```
tcxOpenCV/
├── CMakeLists.txt
├── LICENSE                 # MIT (this addon)
├── README.md               # Usage, OpenCV license notice
├── TODO.md                 # This file
├── src/
│   ├── tcxOpenCV.h         # Main header (includes everything)
│   ├── tcxCvConvert.h      # Type conversion utilities
│   ├── tcxCvContour.h      # Contour detection helpers (optional)
│   └── tcxCvFlow.h         # Optical flow helpers (optional)
├── example-basic/
│   ├── src/tcApp.cpp       # Basic usage demo
│   ├── addons.make
│   └── CMakeLists.txt
└── example-contour/
    └── ...
```

## Implementation Plan

### Phase 1: Core (MVP)
- [ ] Create CMakeLists.txt with FetchContent
- [ ] Implement tcxCvConvert.h
  - [ ] `tcx::toCvMat(tc::Image&)` -> cv::Mat
  - [ ] `tcx::toCvMat(tc::VideoGrabber&)` -> cv::Mat
  - [ ] `tcx::toTcImage(cv::Mat&)` -> tc::Image
  - [ ] `tcx::toCvScalar(tc::Color)` -> cv::Scalar
  - [ ] `tcx::toTcColor(cv::Scalar)` -> tc::Color
  - [ ] `tcx::toCvPoint(tc::Vec2)` -> cv::Point2f
  - [ ] `tcx::toTcVec2(cv::Point2f)` -> tc::Vec2
- [ ] Create example-basic

### Phase 2: Example Collection (Priority!)
The examples ARE the documentation. Each shows cv + tc working together.

- [ ] example-basic - Simple image load/display, type conversion demo
- [ ] example-blur - Blur filters (Gaussian, bilateral, median)
- [ ] example-edge - Edge detection (Canny, Sobel)
- [ ] example-threshold - Thresholding techniques
- [ ] example-contour - Contour detection & drawing
- [ ] example-opticalFlow - Optical flow visualization
- [ ] example-faceDetect - Haar cascade face detection
- [ ] example-backgroundSub - Background subtraction (MOG2)
- [ ] example-colorTracking - Color-based object tracking
- [ ] example-morphology - Erosion, dilation, opening, closing
- [ ] example-histogram - Histogram calculation & equalization
- [ ] example-houghLines - Line detection
- [ ] example-houghCircles - Circle detection
- [ ] example-featureMatch - Feature detection & matching (ORB, SIFT)
- [ ] example-warp - Perspective transform, homography

### Phase 3: Convenience Wrappers (Optional)
Only if patterns emerge from examples that need simplification.

- [ ] tcxCvContour.h - findContours returning vector<vector<Vec2>>
- [ ] tcxCvFlow.h - optical flow with visualization helpers

### Phase 4: Advanced (Maybe)
- [ ] Face detection helper (if Haar is too verbose)
- [ ] ArUco marker detection (needs opencv_contrib)
- [ ] Camera calibration utilities

## tc::VideoGrabber vs cv::VideoCapture

| Feature | tc::VideoGrabber | cv::VideoCapture |
|---------|------------------|------------------|
| Backend | Native (AVFoundation, Media Foundation) | FFmpeg, GStreamer, etc. |
| Output | tc::Texture (GPU ready), tc::Image | cv::Mat |
| Permission handling | Auto (macOS) | Manual |
| TrussC integration | Direct draw() support | Need conversion |
| File playback | No (camera only) | Yes (files, streams, RTSP) |
| Codec support | Limited | Extensive |

**Conclusion**:
- For **webcam**: Use `tc::VideoGrabber` (simpler, native integration)
- For **file/stream/RTSP**: Use `cv::VideoCapture` + conversion
- Provide `toCvMat(VideoGrabber&)` for users who want to process webcam with OpenCV

## ArUco Strategy

**Decision: Separate addon (tcxAruco) depending on tcxOpenCV**

Reasons:
- ArUco is its own domain (marker detection, board management, camera calibration)
- OpenCV 4.11+ has ArUco in main modules (`cv::objdetect`), no contrib needed
- Follows tcxWebSocket → tcxTls dependency pattern
- Users who don't need ArUco get a lighter tcxOpenCV

Future structure:
```
addons/
├── tcxOpenCV/     # This addon - basic cv integration
└── tcxAruco/      # Separate repo, depends on tcxOpenCV
    ├── addons.make: tcxOpenCV
    └── ...
```

## Open Questions

1. **GPU acceleration**: OpenCV has CUDA modules. Include? (probably not - license/complexity)

## License Notice (for README)

```
This addon integrates OpenCV (https://opencv.org/).
OpenCV is licensed under Apache 2.0 License.

Note: Some OpenCV contrib modules have different licenses.
This addon only uses core OpenCV modules (Apache 2.0/BSD).
If you use opencv_contrib modules, please check their individual licenses.
```

## References

- OpenCV docs: https://docs.opencv.org/
- OpenCV GitHub: https://github.com/opencv/opencv
- TrussC ADDONS.md: design patterns for addons
