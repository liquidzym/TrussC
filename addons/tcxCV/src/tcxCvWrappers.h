#pragma once

// ======================================================================
// tcxCvWrappers.h - Image processing wrappers
// ======================================================================
//
// Provides an easy-to-use interface to OpenCV functions using TrussC types.
// Includes in-place and not-in-place variations.
//
// Image operations:
//   Canny, medianBlur, blur, GaussianBlur, convertColor, CLD
//   Sobel, equalizeHist, threshold, normalize, invert, lerp
//   bitwise_and/or/xor, max, min, multiply, divide, add, subtract, absdiff
//   erode, dilate, flip, rotate, resize, warpPerspective
//
// Point set / Polyline functions:
//   convexHull, minAreaRect, fitEllipse, fitLine,
//   convexityDefects, fillConvexPoly, fillPoly
//

#include <TrussC.h>
#include <opencv2/opencv.hpp>
#include <type_traits>
#include "tcxCvUtilities.h"

// Coherent Line Drawing
#include "imatrix.h"
#include "ETF.h"
#include "fdog.h"

namespace tcx {

namespace detail {

template <class A, class B>
bool validateCompatibleMats(const cv::Mat& a, const cv::Mat& b, const char* opName) {
    if (a.empty() || b.empty()) {
        tc::logWarning("tcxCV") << opName << " skipped: empty input";
        return false;
    }
    if (a.size() != b.size() || a.type() != b.type()) {
        tc::logWarning("tcxCV") << opName << " skipped: input size/type mismatch";
        return false;
    }
    return true;
}

template <class S>
int grayConversionFor(const S& src) {
    int channels = getChannels(src);
    if (channels == 4) {
        return cv::COLOR_RGBA2GRAY;
    }
    if (channels == 3) {
        return cv::COLOR_RGB2GRAY;
    }
    return -1;
}

inline int grayConversionFor(const cv::Mat& src) {
    if (src.channels() == 4) {
        return cv::COLOR_BGRA2GRAY;
    }
    if (src.channels() == 3) {
        return cv::COLOR_BGR2GRAY;
    }
    return -1;
}

} // namespace detail

// ======================================================================
// File I/O
// ======================================================================

void loadMat(cv::Mat& mat, const std::string& filename);
void saveMat(cv::Mat mat, const std::string& filename);
void saveImage(cv::Mat& mat, const std::string& filename);

// ======================================================================
// wrapThree - template macro for 3-operand operations
// Operations: max, min, multiply, divide, add, subtract, absdiff,
//             bitwise_and, bitwise_or, bitwise_xor
// ======================================================================

#define wrapThree(name) \
template <class X, class Y, class Result> \
void name(X& x, Y& y, Result& result) { \
    cv::Mat xMat = toCv(x), yMat = toCv(y); \
    if (!detail::validateCompatibleMats<X, Y>(xMat, yMat, #name)) return; \
    imitate(result, x); \
    cv::Mat resultMat = toCv(result); \
    cv::name(xMat, yMat, resultMat); \
    syncToGpu(result); \
}

wrapThree(max)
wrapThree(min)
wrapThree(multiply)
wrapThree(divide)
wrapThree(add)
wrapThree(subtract)
wrapThree(absdiff)
wrapThree(bitwise_and)
wrapThree(bitwise_or)
wrapThree(bitwise_xor)

#undef wrapThree

// ======================================================================
// invert
// ======================================================================

template <class S, class D>
void invert(S& src, D& dst) {
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    cv::bitwise_not(srcMat, dstMat);
    syncToGpu(dst);
}

template <class SD>
void invert(SD& srcDst) {
    invert(srcDst, srcDst);
}

// ======================================================================
// lerp - linear interpolation between two images
// ======================================================================

template <class X, class Y, class R>
void lerp(X& x, Y& y, R& result, float amt = 0.5f) {
    imitate(result, x);
    cv::Mat xMat = toCv(x), yMat = toCv(y);
    cv::Mat resultMat = toCv(result);
    if (yMat.cols == 0) {
        copy(x, result);
    } else if (xMat.cols == 0) {
        copy(y, result);
    } else {
        cv::addWeighted(xMat, amt, yMat, 1.0 - amt, 0.0, resultMat);
    }
    syncToGpu(result);
}

// ======================================================================
// normalize - normalize to [0, max] out of place / in place
// ======================================================================

template <class S, class D>
void normalize(S& src, D& dst) {
    imitate(dst, src);
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    cv::normalize(srcMat, dstMat, 0, getMaxVal(getDepth(dst)), cv::NORM_MINMAX);
    syncToGpu(dst);
}

template <class SD>
void normalize(SD& srcDst) {
    normalize(srcDst, srcDst);
}

// ======================================================================
// threshold - binary threshold out of place / in place
// ======================================================================

template <class S, class D>
void threshold(S& src, D& dst, float thresholdValue, bool invert_ = false) {
    imitate(dst, src);
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    int thresholdType = invert_ ? cv::THRESH_BINARY_INV : cv::THRESH_BINARY;
    float maxVal = getMaxVal(dstMat);
    cv::threshold(srcMat, dstMat, thresholdValue, maxVal, thresholdType);
    syncToGpu(dst);
}

template <class SD>
void threshold(SD& srcDst, float thresholdValue, bool invert_ = false) {
    threshold(srcDst, srcDst, thresholdValue, invert_);
}

// ======================================================================
// erode / dilate
// ======================================================================

template <class S, class D>
void erode(S& src, D& dst, int iterations = 1) {
    imitate(dst, src);
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    cv::erode(srcMat, dstMat, cv::Mat(), cv::Point(-1, -1), iterations);
    syncToGpu(dst);
}

template <class SD>
void erode(SD& srcDst, int iterations = 1) {
    erode(srcDst, srcDst, iterations);
}

template <class S, class D>
void dilate(const S& src, D& dst, int iterations = 1) {
    imitate(dst, src);
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    cv::dilate(srcMat, dstMat, cv::Mat(), cv::Point(-1, -1), iterations);
    syncToGpu(dst);
}

template <class SD>
void dilate(SD& srcDst, int iterations = 1) {
    dilate(srcDst, srcDst, iterations);
}

// ======================================================================
// autothreshold - Otsu automatic threshold (grayscale 8-bit)
// ======================================================================

template <class S, class D>
void autothreshold(const S& src, D& dst, bool invert_ = false) {
    imitate(dst, src);
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    int flags = cv::THRESH_OTSU | (invert_ ? cv::THRESH_BINARY_INV : cv::THRESH_BINARY);
    cv::threshold(srcMat, dstMat, 0, 255, flags);
    syncToGpu(dst);
}

template <class SD>
void autothreshold(SD& srcDst, bool invert_ = false) {
    autothreshold(srcDst, srcDst, invert_);
}

// ======================================================================
// convertColor - convert between color spaces
// ======================================================================

template <class S, class D>
void convertColor(const S& src, D& dst, int code) {
    int targetChannels = getTargetChannelsFromCode(code);
    if (targetChannels <= 0) {
        targetChannels = getChannels(src);
        tc::logWarning("tcxCV") << "convertColor using source channel count for unsupported code " << code;
    }
    imitate(dst, src, getCvImageType(targetChannels, getDepth(src)));
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    cv::cvtColor(srcMat, dstMat, code);
    syncToGpu(dst);
}

cv::Vec3b convertColor(cv::Vec3b color, int code);
tc::Color convertColor(tc::Color color, int code);

// ======================================================================
// copyGray - convert to grayscale efficiently
// ======================================================================

template <class S, class D>
void copyGray(const S& src, D& dst) {
    int channels = getChannels(src);
    int conversion = detail::grayConversionFor(src);
    if (conversion >= 0) {
        convertColor(src, dst, conversion);
    } else if (channels == 1) {
        copy(src, dst);
    } else {
        tc::logWarning("tcxCV") << "copyGray skipped: unsupported channel count " << channels;
    }
}

// ======================================================================
// blur - box blur
// ======================================================================

int forceOdd(int x);

template <class S, class D>
void blur(const S& src, D& dst, int size) {
    imitate(dst, src);
    size = forceOdd(size);
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    cv::blur(srcMat, dstMat, cv::Size(size, size));
    syncToGpu(dst);
}

template <class SD>
void blur(SD& srcDst, int size) {
    blur(srcDst, srcDst, size);
}

// ======================================================================
// GaussianBlur
// ======================================================================

template <class S, class D>
void GaussianBlur(const S& src, D& dst, int size) {
    imitate(dst, src);
    size = forceOdd(size);
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    cv::GaussianBlur(srcMat, dstMat, cv::Size(size, size), 0, 0);
    syncToGpu(dst);
}

template <class SD>
void GaussianBlur(SD& srcDst, int size) {
    GaussianBlur(srcDst, srcDst, size);
}

// ======================================================================
// medianBlur
// ======================================================================

template <class S, class D>
void medianBlur(const S& src, D& dst, int size) {
    imitate(dst, src);
    size = forceOdd(size);
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    cv::medianBlur(srcMat, dstMat, size);
    syncToGpu(dst);
}

template <class SD>
void medianBlur(SD& srcDst, int size) {
    medianBlur(srcDst, srcDst, size);
}

// ======================================================================
// equalizeHist - histogram equalization (supports color images)
// ======================================================================

template <class S, class D>
void equalizeHist(const S& src, D& dst) {
    imitate(dst, src);
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    if (srcMat.channels() > 1) {
        std::vector<cv::Mat> srcEach, dstEach;
        cv::split(srcMat, srcEach);
        cv::split(dstMat, dstEach);
        for (int i = 0; i < (int)srcEach.size(); i++) {
            cv::equalizeHist(srcEach[i], dstEach[i]);
        }
        cv::merge(dstEach, dstMat);
    } else {
        cv::equalizeHist(srcMat, dstMat);
    }
    syncToGpu(dst);
}

template <class SD>
void equalizeHist(SD& srcDst) {
    equalizeHist(srcDst, srcDst);
}

// ======================================================================
// Canny edge detection (output is grayscale 8-bit)
// ======================================================================

template <class S, class D>
void Canny(const S& src, D& dst, double threshold1, double threshold2,
           int apertureSize = 3, bool L2gradient = false) {
    imitate(dst, src, CV_8UC1);
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    cv::Canny(srcMat, dstMat, threshold1, threshold2, apertureSize, L2gradient);
    syncToGpu(dst);
}

// ======================================================================
// Sobel edge detection
// ======================================================================

template <class S, class D>
void Sobel(const S& src, D& dst,
           int ddepth = -1, int dx = 1, int dy = 1,
           int ksize = 3, double scale = 1.0, double delta = 0.0,
           int borderType = cv::BORDER_DEFAULT) {
    imitate(dst, src, CV_8UC1);
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    cv::Sobel(srcMat, dstMat, ddepth, dx, dy, ksize, scale, delta, borderType);
    syncToGpu(dst);
}

// ======================================================================
// CLD - Coherent Line Drawing
// Good values: halfw 1-8, smoothPasses 1-4, sigma1 0.01-2.0,
//              sigma2 0.01-10.0, tau 0.8-1.0
// ======================================================================

template <class S, class D>
void CLD(const S& src, D& dst,
         int halfw = 4, int smoothPasses = 2,
         double sigma1 = 0.4, double sigma2 = 3.0,
         double tau = 0.97, int black = 0) {
    int width = getWidth(src), height = getHeight(src);
    cv::Mat gray;
    copyGray(src, gray);
    if (gray.empty()) {
        tc::logWarning("tcxCV") << "CLD skipped: empty or unsupported input";
        return;
    }

    imatrix img;
    img.init(height, width);

    allocate(dst, width, height, CV_8UC1);
    cv::Mat dstMat = toCv(dst);
    gray.copyTo(dstMat);
    if (black != 0) {
        cv::add(dstMat, cv::Scalar(black), dstMat);
    }
    // Copy dst (uchar) to img (int)
    for (int y = 0; y < height; ++y) {
        const unsigned char* dstPtr = dstMat.ptr<unsigned char>(y);
        for (int x = 0; x < width; ++x) {
            img[y][x] = dstPtr[x];
        }
    }
    ETF etf;
    etf.init(height, width);
    etf.set(img);
    etf.Smooth(halfw, smoothPasses);
    GetFDoG(img, etf, sigma1, sigma2, tau);
    // Copy result from img (int) to dst (uchar)
    for (int y = 0; y < height; ++y) {
        unsigned char* dstPtr = dstMat.ptr<unsigned char>(y);
        for (int x = 0; x < width; ++x) {
            dstPtr[x] = cv::saturate_cast<unsigned char>(img[y][x]);
        }
    }
    syncToGpu(dst);
}

// ======================================================================
// warpPerspective / unwarpPerspective
// ======================================================================

template <class S, class D>
void warpPerspective(const S& src, D& dst, std::vector<cv::Point2f>& dstPoints,
                     int flags = cv::INTER_LINEAR) {
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    int w = srcMat.cols;
    int h = srcMat.rows;
    std::vector<cv::Point2f> srcPoints(4);
    srcPoints[0] = cv::Point2f(0, 0);
    srcPoints[1] = cv::Point2f((float)w, 0);
    srcPoints[2] = cv::Point2f((float)w, (float)h);
    srcPoints[3] = cv::Point2f(0, (float)h);
    cv::Mat transform = cv::getPerspectiveTransform(&srcPoints[0], &dstPoints[0]);
    cv::warpPerspective(srcMat, dstMat, transform, dstMat.size(), flags);
    syncToGpu(dst);
}

template <class S, class D>
void unwarpPerspective(const S& src, D& dst, std::vector<cv::Point2f>& srcPoints,
                       int flags = cv::INTER_LINEAR) {
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    int w = dstMat.cols;
    int h = dstMat.rows;
    std::vector<cv::Point2f> dstPoints(4);
    dstPoints[0] = cv::Point2f(0, 0);
    dstPoints[1] = cv::Point2f((float)w, 0);
    dstPoints[2] = cv::Point2f((float)w, (float)h);
    dstPoints[3] = cv::Point2f(0, (float)h);
    cv::Mat transform = cv::getPerspectiveTransform(&srcPoints[0], &dstPoints[0]);
    cv::warpPerspective(srcMat, dstMat, transform, dstMat.size(), flags);
    syncToGpu(dst);
}

template <class S, class D>
void warpPerspective(const S& src, D& dst, cv::Mat& transform,
                     int flags = cv::INTER_LINEAR) {
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    cv::warpPerspective(srcMat, dstMat, transform, dstMat.size(), flags);
    syncToGpu(dst);
}

// ======================================================================
// resize
// ======================================================================

template <class S, class D>
void resize(const S& src, D& dst, int interpolation = cv::INTER_LINEAR) {
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    cv::resize(srcMat, dstMat, dstMat.size(), 0, 0, interpolation);
    syncToGpu(dst);
}

template <class S, class D>
void resize(const S& src, D& dst, float xScale, float yScale,
            int interpolation = cv::INTER_LINEAR) {
    int dstWidth = (int)(getWidth(src) * xScale);
    int dstHeight = (int)(getHeight(src) * yScale);
    if (getWidth(dst) != dstWidth || getHeight(dst) != dstHeight) {
        allocate(dst, dstWidth, dstHeight, getCvImageType(src));
    }
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    cv::resize(srcMat, dstMat, dstMat.size(), 0, 0, interpolation);
    syncToGpu(dst);
}

// ======================================================================
// Polyline / contour geometry
// ======================================================================

tc::Path convexHull(const tc::Path& path);
std::vector<cv::Vec4i> convexityDefects(const std::vector<cv::Point>& contour);
std::vector<cv::Vec4i> convexityDefects(const tc::Path& path);
cv::RotatedRect minAreaRect(const tc::Path& path);
cv::RotatedRect fitEllipse(const tc::Path& path);
void fitLine(const tc::Path& path, tc::Vec2& point, tc::Vec2& direction);

// fillConvexPoly - fills a convex polygon into image (much faster than fillPoly)
template <class D>
void fillConvexPoly(const std::vector<cv::Point>& points, D& dst) {
    cv::Mat dstMat = toCv(dst);
    dstMat.setTo(cv::Scalar(0));
    cv::fillConvexPoly(dstMat, points, cv::Scalar(255));
}

// fillPoly - fills area bounded by one or more polygons (handles holes, etc.)
template <class D>
void fillPoly(const std::vector<cv::Point>& points, D& dst) {
    cv::Mat dstMat = toCv(dst);
    dstMat.setTo(cv::Scalar(0));
    cv::fillPoly(dstMat, points, cv::Scalar(255));
}

// ======================================================================
// flip / rotate
// ======================================================================

template <class S, class D>
void flip(const S& src, D& dst, int code) {
    imitate(dst, src);
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    cv::flip(srcMat, dstMat, code);
    syncToGpu(dst);
}

template <class S, class D>
void rotate(const S& src, D& dst, double angle,
            tc::Color fill = tc::Color(0, 0, 0, 1),
            int interpolation = cv::INTER_LINEAR) {
    imitate(dst, src);
    cv::Mat srcMat = toCv(src), dstMat = toCv(dst);
    cv::Point2f center((float)(srcMat.cols / 2.0), (float)(srcMat.rows / 2.0));
    cv::Mat rotationMatrix = cv::getRotationMatrix2D(center, angle, 1.0);
    cv::warpAffine(srcMat, dstMat, rotationMatrix, srcMat.size(),
                   interpolation, cv::BORDER_CONSTANT, toCv(fill));
    syncToGpu(dst);
}

// rotate90 - efficient rotation for multiples of 90 degrees
template <class S, class D>
void rotate90(const S& src, D& dst, int angle) {
    cv::Mat srcMat = toCv(src);
    if (angle == 0 || angle == 360 || angle == -360) {
        copy(src, dst);
        return;
    } else if (angle == 90 || angle == -270) {
        allocate(dst, srcMat.rows, srcMat.cols, srcMat.type());
        cv::Mat dstMat = toCv(dst);
        cv::transpose(srcMat, dstMat);
        cv::flip(dstMat, dstMat, 1);
    } else if (angle == 180 || angle == -180) {
        imitate(dst, src);
        cv::Mat dstMat = toCv(dst);
        cv::flip(srcMat, dstMat, -1);
    } else if (angle == 270 || angle == -90) {
        allocate(dst, srcMat.rows, srcMat.cols, srcMat.type());
        cv::Mat dstMat = toCv(dst);
        cv::transpose(srcMat, dstMat);
        cv::flip(dstMat, dstMat, 0);
    }
    syncToGpu(dst);
}

// transpose
template <class S, class D>
void transpose(const S& src, D& dst) {
    cv::Mat srcMat = toCv(src);
    allocate(dst, srcMat.rows, srcMat.cols, srcMat.type());
    cv::Mat dstMat = toCv(dst);
    cv::transpose(srcMat, dstMat);
    syncToGpu(dst);
}

// ======================================================================
// 3D affine estimation
// ======================================================================

tc::Mat4 estimateAffine3D(std::vector<tc::Vec3>& from, std::vector<tc::Vec3>& to,
                          float accuracy = 0.99f);
tc::Mat4 estimateAffine3D(std::vector<tc::Vec3>& from, std::vector<tc::Vec3>& to,
                          std::vector<unsigned char>& outliers, float accuracy = 0.99f);

} // namespace tcx
