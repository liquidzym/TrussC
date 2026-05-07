# tcxOpenCV

OpenCV integration addon for [TrussC](https://trussc.org).

This addon provides minimal glue between TrussC and OpenCV - type conversion utilities that let you use OpenCV's powerful image processing with TrussC's rendering.

## Installation

Clone this repository into your TrussC `addons/` folder:

```bash
cd path/to/TrussC/addons
git clone https://github.com/TrussC-org/tcxOpenCV.git
```

Then add `tcxOpenCV` to your project's `addons.make`:

```
tcxOpenCV
```

That's it! OpenCV 4.10.0 will be automatically fetched and built on first compile.

## Usage

```cpp
#include <TrussC.h>
#include <tcxOpenCV.h>

using namespace std;
using namespace tc;

class tcApp : public App {
    Image img;
    Image processed;

    void setup() override {
        img.load("photo.jpg");
    }

    void update() override {
        // Convert tc::Image to cv::Mat
        cv::Mat mat = tcx::toCvMat(img);

        // Use OpenCV as usual
        cv::Mat blurred;
        cv::GaussianBlur(mat, blurred, cv::Size(15, 15), 0);

        // Convert back to tc::Image
        tcx::toTcImage(blurred, processed);
    }

    void draw() override {
        processed.draw(0, 0);
    }
};
```

## API Reference

### Image Conversion

| Function | Description |
|----------|-------------|
| `tcx::toCvMat(tc::Image&)` | Convert tc::Image to cv::Mat (BGR) |
| `tcx::toCvMat(tc::VideoGrabber&)` | Convert VideoGrabber frame to cv::Mat |
| `tcx::toTcImage(cv::Mat&)` | Convert cv::Mat to new tc::Image |
| `tcx::toTcImage(cv::Mat&, tc::Image&)` | Convert cv::Mat into existing tc::Image |

### Type Conversion

| Function | Description |
|----------|-------------|
| `tcx::toCvScalar(tc::Color)` | tc::Color (RGBA 0-1) to cv::Scalar (BGR 0-255) |
| `tcx::toTcColor(cv::Scalar)` | cv::Scalar to tc::Color |
| `tcx::toCvPoint(tc::Vec2)` | tc::Vec2 to cv::Point2f |
| `tcx::toCvPointI(tc::Vec2)` | tc::Vec2 to cv::Point (integer) |
| `tcx::toTcVec2(cv::Point2f)` | cv::Point2f to tc::Vec2 |
| `tcx::toCvRect(tc::Rect)` | tc::Rect to cv::Rect |
| `tcx::toTcRect(cv::Rect)` | cv::Rect to tc::Rect |

### Contour Helper

| Function | Description |
|----------|-------------|
| `tcx::toTcContours(vector<vector<cv::Point>>&)` | Convert cv contours to `vector<vector<tc::Vec2>>` |

## Design Philosophy

This addon provides **minimal glue**, not a full wrapper:

- Use `cv::` directly for image processing - OpenCV has extensive documentation
- Only type conversion is wrapped in `tcx::`
- TrussC types (Image, Color, Vec2) work seamlessly with OpenCV

## OpenCV Modules Included

- `core` - Basic structures
- `imgproc` - Image processing (blur, edge detection, etc.)
- `imgcodecs` - Image file I/O
- `videoio` - Video capture
- `objdetect` - Object detection (including ArUco markers)
- `features2d` - Feature detection
- `calib3d` - Camera calibration, 3D reconstruction

## Requirements

- TrussC (https://github.com/TrussC-org/TrussC)
- CMake 3.20+
- C++20 compiler

## License

MIT License - see [LICENSE](LICENSE)

This addon integrates [OpenCV](https://opencv.org/), which is licensed under Apache 2.0 License.

## Links

- TrussC: https://trussc.org
- TrussC GitHub: https://github.com/TrussC-org/TrussC
- OpenCV: https://opencv.org
