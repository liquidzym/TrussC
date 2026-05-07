# tcxCV - Computer Vision Addon for TrussC

A comprehensive computer vision addon for TrussC, ported from [ofxCv](https://github.com/daitomanabe/ofxCv). Provides image processing wrappers, contour finding, optical flow, camera calibration, object tracking, and more.

## Dependencies

- [tcxOpenCV](https://github.com/TrussC-org/tcxOpenCV) - OpenCV integration (must be in addons folder)
- OpenCV 4.12.0 (fetched automatically via tcxOpenCV)

## Modules

| Module | Description |
|--------|-------------|
| **Utilities** | Type conversion between TrussC and OpenCV (`toCv()`, `toOf()`, `imitate()`, `copy()`) |
| **Wrappers** | Image operations: blur, Canny, threshold, erode/dilate, warp, rotate, resize, CLD |
| **Helpers** | Matrix building, thinning, contour simplification, drawing |
| **ContourFinder** | Contour detection with area filtering, color tracking, hole detection |
| **Tracker** | Generic object tracking across frames (RectTracker, PointTracker) |
| **Flow** | Optical flow: sparse PyrLK and dense Farneback |
| **Kalman** | Kalman filter for position and orientation smoothing |
| **Calibration** | Camera intrinsic calibration and undistortion |
| **RunningBackground** | Background subtraction via running average |
| **ObjectFinder** | Cascade classifier object detection (face detection, etc.) |
| **Distance** | Edit distance and string correlation utilities |

## Quick Start

```cpp
#include <tcxCV.h>
using namespace std;
using namespace tc;
using namespace tcx;

// Image processing
Image img;
img.load("photo.jpg");
cv::Mat mat = toCv(img);
tcx::GaussianBlur(mat, mat, 5);
tcx::Canny(mat, mat, 50, 200);
toOf(mat, img);

// Contour finding
ContourFinder finder;
finder.setMinAreaRadius(10);
finder.setMaxAreaRadius(200);
finder.findContours(img);
for (unsigned int i = 0; i < finder.size(); i++) {
    Polyline& poly = finder.getPolyline(i);
    cv::Rect box = finder.getBoundingRect(i);
}

// Optical flow
FlowPyrLK flow;
flow.calcOpticalFlow(prevImg, curImg);
vector<Vec2> motion = flow.getMotion();

// Object detection
ObjectFinder faceFinder;
faceFinder.setup("haarcascade_frontalface_default.xml");
faceFinder.setPreset(ObjectFinder::Fast);
faceFinder.update(img);
for (unsigned int i = 0; i < faceFinder.size(); i++) {
    Rect obj = faceFinder.getObject(i);
}
```

## Platform Support

- macOS (x86_64, arm64)
- iOS
- Windows
- Android
- Web (Emscripten)

Cross-platform support is provided through CMake and OpenCV's platform abstractions.

## License

MIT License
