#pragma once

// ======================================================================
// tcxCvUtilities.h - Low-level conversion utilities
// ======================================================================
//
// Provides toCv() and toOf() conversion functions between TrussC types
// and OpenCV types. Also provides imitate(), allocate(), copy() for
// type-safe image allocation.
//
// Extends tcxOpenCV's conversion functions with additional types:
//   - Path <-> vector<cv::Point2f>
//   - Vec3 <-> cv::Point3f
//   - vector<Vec2> <-> vector<cv::Point2f>
//   - cv::RotatedRect -> Polyline
//   - Mat depth/channel helpers
//

#include <TrussC.h>
#include <tcxOpenCV.h>
#include <opencv2/opencv.hpp>

namespace tcx {

// ======================================================================
// toCv() - Convert TrussC types to OpenCV types
// ======================================================================

// cv::Mat pass-through
inline cv::Mat toCv(cv::Mat& mat) {
    return mat;
}

inline cv::Mat toCv(const cv::Mat& mat) {
    return mat.clone();
}

// tc::Vec2 -> cv::Point2f
inline cv::Point2f toCv(tc::Vec2 vec) {
    return cv::Point2f(vec.x, vec.y);
}

// tc::Vec3 -> cv::Point3f
inline cv::Point3f toCv(tc::Vec3 vec) {
    return cv::Point3f(vec.x, vec.y, vec.z);
}

// tc::Rect -> cv::Rect
inline cv::Rect toCv(tc::Rect rect) {
    return cv::Rect(
        static_cast<int>(rect.x),
        static_cast<int>(rect.y),
        static_cast<int>(rect.width),
        static_cast<int>(rect.height)
    );
}

// tc::Path -> vector<cv::Point2f>
inline std::vector<cv::Point2f> toCv(const tc::Path& path) {
    std::vector<cv::Point2f> contour;
    contour.reserve(path.size());
    for (int i = 0; i < path.size(); i++) {
        tc::Vec3 v = path[i];
        contour.push_back(cv::Point2f(v.x, v.y));
    }
    return contour;
}

// vector<tc::Vec2> -> vector<cv::Point2f>
inline std::vector<cv::Point2f> toCv(const std::vector<tc::Vec2>& points) {
    std::vector<cv::Point2f> out;
    out.reserve(points.size());
    for (size_t i = 0; i < points.size(); i++) {
        out.push_back(cv::Point2f(points[i].x, points[i].y));
    }
    return out;
}

// vector<tc::Vec3> -> vector<cv::Point3f>
inline std::vector<cv::Point3f> toCv(const std::vector<tc::Vec3>& points) {
    std::vector<cv::Point3f> out;
    out.reserve(points.size());
    for (size_t i = 0; i < points.size(); i++) {
        out.push_back(cv::Point3f(points[i].x, points[i].y, points[i].z));
    }
    return out;
}

// tc::Color -> cv::Scalar (BGR 0-255)
inline cv::Scalar toCv(tc::Color color) {
    return cv::Scalar(
        static_cast<int>(color.b * 255),
        static_cast<int>(color.g * 255),
        static_cast<int>(color.r * 255),
        static_cast<int>(color.a * 255)
    );
}

// tc::Image -> cv::Mat (wraps shared memory as RGBA, no copy)
// Modifications to the returned Mat directly affect the Image's pixels.
// After modifying, call img.setDirty() and img.update() to sync to GPU.
// For a BGR copy, use tcx::toCvMat() instead.
inline cv::Mat toCv(tc::Image& img) {
    if (!img.isAllocated()) return cv::Mat();
    return cv::Mat(img.getHeight(), img.getWidth(), CV_8UC4,
                   img.getPixelsData());
}

// const tc::Image -> cv::Mat (makes a clone for safety)
inline cv::Mat toCv(const tc::Image& img) {
    if (!img.isAllocated()) return cv::Mat();
    cv::Mat rgba(img.getHeight(), img.getWidth(), CV_8UC4,
                 const_cast<unsigned char*>(img.getPixelsData()));
    return rgba.clone();
}

// ======================================================================
// toOf() / toTc() - Convert OpenCV types to TrussC types
// ======================================================================

// cv::Point2f -> tc::Vec2
inline tc::Vec2 toOf(cv::Point2f point) {
    return tc::Vec2(point.x, point.y);
}

// cv::Point3f -> tc::Vec3
inline tc::Vec3 toOf(cv::Point3f point) {
    return tc::Vec3(point.x, point.y, point.z);
}

// cv::Rect -> tc::Rect
inline tc::Rect toOf(cv::Rect rect) {
    return tc::Rect(
        static_cast<float>(rect.x),
        static_cast<float>(rect.y),
        static_cast<float>(rect.width),
        static_cast<float>(rect.height)
    );
}

// vector<cv::Point2f> -> tc::Path
inline tc::Path toOf(const std::vector<cv::Point2f>& points) {
    tc::Path path;
    for (size_t i = 0; i < points.size(); i++) {
        path.addVertex(points[i].x, points[i].y);
    }
    return path;
}

// vector<cv::Point> -> tc::Path
inline tc::Path toOf(const std::vector<cv::Point>& points) {
    tc::Path path;
    for (size_t i = 0; i < points.size(); i++) {
        path.addVertex(
            static_cast<float>(points[i].x),
            static_cast<float>(points[i].y)
        );
    }
    return path;
}

// cv::RotatedRect -> tc::Path (4 corners)
inline tc::Path toOf(cv::RotatedRect rect) {
    std::vector<cv::Point2f> corners(4);
    rect.points(&corners[0]);
    return toOf(corners);
}

// cv::Mat -> tc::Image (delegates to tcxOpenCV)
inline tc::Image toOf(cv::Mat mat) {
    return tcx::toTcImage(mat);
}

inline void toOf(cv::Mat mat, tc::Image& img) {
    tcx::toTcImage(mat, img);
}

// ======================================================================
// Image property helpers
// ======================================================================

// Get image width (works with both tc::Image and cv::Mat)
inline int getWidth(const tc::Image& img) {
    return img.getWidth();
}

inline int getWidth(const cv::Mat& mat) {
    return mat.cols;
}

// Get image height
inline int getHeight(const tc::Image& img) {
    return img.getHeight();
}

inline int getHeight(const cv::Mat& mat) {
    return mat.rows;
}

// Get number of channels
inline int getChannels(const tc::Image& img) {
    return img.getChannels();
}

inline int getChannels(const cv::Mat& mat) {
    return mat.channels();
}

// Get cv::Mat depth (CV_8U, CV_32F, etc.)
inline int getDepth(const tc::Image&) {
    return CV_8U;
}

inline int getDepth(const cv::Mat& mat) {
    return mat.depth();
}

// Get maximum value for a given cv depth
inline float getMaxVal(int cvDepth) {
    switch (cvDepth) {
        case CV_8U:  return (float)std::numeric_limits<uint8_t>::max();
        case CV_16U: return (float)std::numeric_limits<uint16_t>::max();
        case CV_8S:  return (float)std::numeric_limits<int8_t>::max();
        case CV_16S: return (float)std::numeric_limits<int16_t>::max();
        case CV_32S: return (float)std::numeric_limits<int32_t>::max();
        case CV_32F: return 1.0f;
        case CV_64F:
        default:     return 1.0f;
    }
}

inline float getMaxVal(const cv::Mat& mat) {
    return getMaxVal(mat.depth());
}

// Get the target number of channels for a given cvtColor code
inline int getTargetChannelsFromCode(int conversionCode) {
    switch (conversionCode) {
        case cv::COLOR_RGB2RGBA:
        case cv::COLOR_GRAY2RGBA:
        case cv::COLOR_BGRA2RGBA:
        case cv::COLOR_BGR5652BGRA:
        case cv::COLOR_BGR5652RGBA:
        case cv::COLOR_BGR5552BGRA:
        case cv::COLOR_BGR5552RGBA:
        case cv::COLOR_Lab2LBGR:
        case cv::COLOR_Lab2LRGB:
        case cv::COLOR_Luv2LBGR:
        case cv::COLOR_Luv2LRGB:
            return 4;
        case cv::COLOR_BGRA2BGR:
        case cv::COLOR_BGR2RGB:
        case cv::COLOR_GRAY2RGB:
        case cv::COLOR_BGR2GRAY:
        case cv::COLOR_RGB2GRAY:
        case cv::COLOR_BGRA2GRAY:
        case cv::COLOR_RGBA2GRAY:
        case cv::COLOR_BGR5652BGR:
        case cv::COLOR_BGR5652RGB:
        case cv::COLOR_BGR5552BGR:
        case cv::COLOR_BGR5552RGB:
        case cv::COLOR_BGR2XYZ:
        case cv::COLOR_RGB2XYZ:
        case cv::COLOR_XYZ2BGR:
        case cv::COLOR_XYZ2RGB:
        case cv::COLOR_BGR2YCrCb:
        case cv::COLOR_RGB2YCrCb:
        case cv::COLOR_YCrCb2BGR:
        case cv::COLOR_YCrCb2RGB:
        case cv::COLOR_BGR2HSV:
        case cv::COLOR_RGB2HSV:
        case cv::COLOR_BGR2Lab:
        case cv::COLOR_RGB2Lab:
        case cv::COLOR_BayerGB2BGR:
        case cv::COLOR_BayerBG2RGB:
        case cv::COLOR_BayerGB2RGB:
        case cv::COLOR_BayerRG2RGB:
        case cv::COLOR_BGR2Luv:
        case cv::COLOR_RGB2Luv:
        case cv::COLOR_BGR2HLS:
        case cv::COLOR_RGB2HLS:
        case cv::COLOR_HSV2BGR:
        case cv::COLOR_HSV2RGB:
        case cv::COLOR_Lab2BGR:
        case cv::COLOR_Lab2RGB:
        case cv::COLOR_Luv2BGR:
        case cv::COLOR_Luv2RGB:
        case cv::COLOR_HLS2BGR:
        case cv::COLOR_HLS2RGB:
        case cv::COLOR_BayerBG2RGB_VNG:
        case cv::COLOR_BayerGB2RGB_VNG:
        case cv::COLOR_BayerRG2RGB_VNG:
        case cv::COLOR_BayerGR2RGB_VNG:
        case cv::COLOR_BGR2HSV_FULL:
        case cv::COLOR_RGB2HSV_FULL:
        case cv::COLOR_BGR2HLS_FULL:
        case cv::COLOR_RGB2HLS_FULL:
        case cv::COLOR_HSV2BGR_FULL:
        case cv::COLOR_HSV2RGB_FULL:
        case cv::COLOR_HLS2BGR_FULL:
        case cv::COLOR_HLS2RGB_FULL:
        case cv::COLOR_LBGR2Lab:
        case cv::COLOR_LRGB2Lab:
        case cv::COLOR_LBGR2Luv:
        case cv::COLOR_LRGB2Luv:
        case cv::COLOR_BGR2YUV:
        case cv::COLOR_RGB2YUV:
        case cv::COLOR_YUV2BGR:
        case cv::COLOR_YUV2RGB:
            return 3;
        case cv::COLOR_BGR5652GRAY:
        case cv::COLOR_BGR5552GRAY:
            return 1;
        default:
            return 0;
    }
}

// Get OpenCV image type (CV_8UC3, etc.) from channels and depth
inline int getCvImageType(int channels, int depth) {
    return CV_MAKETYPE(depth, channels);
}

inline int getCvImageType(const tc::Image& img) {
    return CV_MAKETYPE(CV_8U, img.getChannels());
}

inline int getCvImageType(const cv::Mat& mat) {
    return mat.type();
}

// ======================================================================
// imitate() - Allocate a destination to match source dimensions/type
// ======================================================================

// Imitate: allocate dst Image to match src Image
inline void imitate(tc::Image& dst, const tc::Image& src) {
    dst.allocate(src.getWidth(), src.getHeight(), src.getChannels());
    dst.update();
}

// Imitate: allocate dst Image to match src Mat (converts to RGBA)
inline void imitate(tc::Image& dst, const cv::Mat& src) {
    dst.allocate(src.cols, src.rows, 4);
    dst.update();
}

// Imitate: allocate dst Mat to match src Image
inline void imitate(cv::Mat& dst, const tc::Image& src) {
    if (!src.isAllocated()) return;
    int type = CV_MAKETYPE(CV_8U, src.getChannels());
    dst = cv::Mat(src.getHeight(), src.getWidth(), type);
}

// Imitate: allocate dst Mat to match src Mat
inline void imitate(cv::Mat& dst, const cv::Mat& src) {
    dst = cv::Mat(src.rows, src.cols, src.type());
}

// Imitate with specific type
inline void imitate(cv::Mat& dst, const tc::Image& src, int cvType) {
    if (!src.isAllocated()) return;
    dst = cv::Mat(src.getHeight(), src.getWidth(), cvType);
}

inline void imitate(cv::Mat& dst, const cv::Mat& src, int cvType) {
    dst = cv::Mat(src.rows, src.cols, cvType);
}

inline void imitate(tc::Image& dst, const tc::Image& src, int /*cvType*/) {
    // tc::Image is always RGBA 8-bit, cvType is ignored
    dst.allocate(src.getWidth(), src.getHeight(), 4);
    dst.update();
}

inline void imitate(tc::Image& dst, const cv::Mat& src, int /*cvType*/) {
    // tc::Image is always RGBA 8-bit
    dst.allocate(src.cols, src.rows, 4);
    dst.update();
}

// ======================================================================
// allocate() - Allocate an image to specific dimensions
// ======================================================================

inline void allocate(tc::Image& img, int width, int height, int /*cvType*/) {
    img.allocate(width, height, 4);
    img.update();
}

inline void allocate(cv::Mat& mat, int width, int height, int cvType) {
    mat = cv::Mat(height, width, cvType);
}

// ======================================================================
// copy() - Copy image data
// ======================================================================

// Copy between cv::Mat
inline void copy(const cv::Mat& src, cv::Mat& dst) {
    src.copyTo(dst);
}

// Copy from cv::Mat to tc::Image (uploads to GPU)
inline void copy(const cv::Mat& src, tc::Image& dst) {
    toOf(src, dst);
}

// ======================================================================
// GPU sync helper - call after modifying Image through shared-memory Mat
// ======================================================================

namespace detail {
    // Type trait to detect tc::Image
    template<typename T> struct is_tc_image : std::false_type {};
    template<> struct is_tc_image<tc::Image> : std::true_type {};

    template<typename T>
    inline void syncToGpu(T& dst, std::false_type) {
        // No-op for non-Image types (cv::Mat, etc.)
    }

    template<typename T>
    inline void syncToGpu(T& dst, std::true_type) {
        dst.setDirty();
        dst.update();
    }
}

// Call this after modifying an Image through a shared-memory toCv() Mat.
// For cv::Mat destinations, this is a no-op.
template<typename T>
inline void syncToGpu(T& dst) {
    detail::syncToGpu(dst, detail::is_tc_image<T>{});
}

// Copy from tc::Image to cv::Mat
inline void copy(const tc::Image& src, cv::Mat& dst) {
    dst = toCv(src);
}

// Copy between tc::Image
inline void copy(const tc::Image& src, tc::Image& dst) {
    imitate(dst, src);
    cv::Mat srcMat = toCv(src);
    cv::Mat dstMat = toCv(dst);
    srcMat.copyTo(dstMat);
}

} // namespace tcx
