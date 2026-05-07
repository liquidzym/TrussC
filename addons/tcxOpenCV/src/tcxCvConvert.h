#pragma once

// =============================================================================
// tcxCvConvert.h - Type conversion between TrussC and OpenCV
// =============================================================================

#include <TrussC.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace tcx {

// =============================================================================
// Image/Mat conversion
// =============================================================================

// Convert tc::Image to cv::Mat (makes a copy, BGR format)
inline cv::Mat toCvMat(const tc::Image& image) {
    if (!image.isAllocated()) {
        return cv::Mat();
    }

    int w = image.getWidth();
    int h = image.getHeight();
    int ch = image.getChannels();

    if (ch == 4) {
        // RGBA -> BGR (OpenCV default)
        cv::Mat rgba(h, w, CV_8UC4, const_cast<unsigned char*>(image.getPixelsData()));
        cv::Mat bgr;
        cv::cvtColor(rgba, bgr, cv::COLOR_RGBA2BGR);
        return bgr;
    } else if (ch == 3) {
        // RGB -> BGR
        cv::Mat rgb(h, w, CV_8UC3, const_cast<unsigned char*>(image.getPixelsData()));
        cv::Mat bgr;
        cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
        return bgr;
    } else if (ch == 1) {
        // Grayscale - direct copy
        cv::Mat gray(h, w, CV_8UC1, const_cast<unsigned char*>(image.getPixelsData()));
        return gray.clone();
    }

    return cv::Mat();
}

// Convert tc::Image to cv::Mat (RGBA format, shares data if possible)
// Use this when you need RGBA format or want to avoid color conversion
inline cv::Mat toCvMatRGBA(const tc::Image& image) {
    if (!image.isAllocated()) {
        return cv::Mat();
    }

    int w = image.getWidth();
    int h = image.getHeight();
    int ch = image.getChannels();

    if (ch == 4) {
        // Direct wrap (no copy, but be careful with lifetime!)
        return cv::Mat(h, w, CV_8UC4, const_cast<unsigned char*>(image.getPixelsData())).clone();
    } else if (ch == 1) {
        return cv::Mat(h, w, CV_8UC1, const_cast<unsigned char*>(image.getPixelsData())).clone();
    }

    return cv::Mat();
}

// Convert tc::VideoGrabber frame to cv::Mat (BGR format)
inline cv::Mat toCvMat(tc::VideoGrabber& grabber) {
    if (!grabber.isInitialized()) {
        return cv::Mat();
    }

    int w = grabber.getWidth();
    int h = grabber.getHeight();

    // VideoGrabber always outputs RGBA
    cv::Mat rgba(h, w, CV_8UC4, grabber.getPixels());
    cv::Mat bgr;
    cv::cvtColor(rgba, bgr, cv::COLOR_RGBA2BGR);
    return bgr;
}

// Convert cv::Mat to tc::Image (copies data)
inline void toTcImage(const cv::Mat& mat, tc::Image& image) {
    if (mat.empty()) {
        return;
    }

    int w = mat.cols;
    int h = mat.rows;
    int ch = mat.channels();

    // Allocate as RGBA (TrussC default)
    image.allocate(w, h, 4);
    unsigned char* dst = image.getPixelsData();

    if (ch == 3) {
        // BGR -> RGBA
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGR2RGBA);
        memcpy(dst, rgba.data, w * h * 4);
    } else if (ch == 4) {
        // BGRA -> RGBA
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_BGRA2RGBA);
        memcpy(dst, rgba.data, w * h * 4);
    } else if (ch == 1) {
        // Grayscale -> RGBA
        cv::Mat rgba;
        cv::cvtColor(mat, rgba, cv::COLOR_GRAY2RGBA);
        memcpy(dst, rgba.data, w * h * 4);
    }

    image.setDirty();
    image.update();
}

// Convenience: create and return new Image
inline tc::Image toTcImage(const cv::Mat& mat) {
    tc::Image image;
    toTcImage(mat, image);
    return image;
}

// =============================================================================
// Color/Scalar conversion
// =============================================================================

// tc::Color (RGBA 0-1) -> cv::Scalar (BGR 0-255)
inline cv::Scalar toCvScalar(const tc::Color& c) {
    return cv::Scalar(
        static_cast<int>(c.b * 255),
        static_cast<int>(c.g * 255),
        static_cast<int>(c.r * 255),
        static_cast<int>(c.a * 255)
    );
}

// cv::Scalar (BGR 0-255) -> tc::Color (RGBA 0-1)
inline tc::Color toTcColor(const cv::Scalar& s) {
    return tc::Color(
        static_cast<float>(s[2]) / 255.0f,  // R
        static_cast<float>(s[1]) / 255.0f,  // G
        static_cast<float>(s[0]) / 255.0f,  // B
        static_cast<float>(s[3]) / 255.0f   // A
    );
}

// =============================================================================
// Point/Vec2 conversion
// =============================================================================

// tc::Vec2 -> cv::Point2f
inline cv::Point2f toCvPoint(const tc::Vec2& v) {
    return cv::Point2f(v.x, v.y);
}

// tc::Vec2 -> cv::Point (integer)
inline cv::Point toCvPointI(const tc::Vec2& v) {
    return cv::Point(static_cast<int>(v.x), static_cast<int>(v.y));
}

// cv::Point2f -> tc::Vec2
inline tc::Vec2 toTcVec2(const cv::Point2f& p) {
    return tc::Vec2(p.x, p.y);
}

// cv::Point -> tc::Vec2
inline tc::Vec2 toTcVec2(const cv::Point& p) {
    return tc::Vec2(static_cast<float>(p.x), static_cast<float>(p.y));
}

// =============================================================================
// Rect conversion
// =============================================================================

// tc::Rect -> cv::Rect
inline cv::Rect toCvRect(const tc::Rect& r) {
    return cv::Rect(
        static_cast<int>(r.x),
        static_cast<int>(r.y),
        static_cast<int>(r.width),
        static_cast<int>(r.height)
    );
}

// cv::Rect -> tc::Rect
inline tc::Rect toTcRect(const cv::Rect& r) {
    return tc::Rect(
        static_cast<float>(r.x),
        static_cast<float>(r.y),
        static_cast<float>(r.width),
        static_cast<float>(r.height)
    );
}

// =============================================================================
// Contour conversion helper
// =============================================================================

// Convert cv contours to tc::Vec2 vectors
inline std::vector<std::vector<tc::Vec2>> toTcContours(
    const std::vector<std::vector<cv::Point>>& cvContours
) {
    std::vector<std::vector<tc::Vec2>> result;
    result.reserve(cvContours.size());

    for (const auto& contour : cvContours) {
        std::vector<tc::Vec2> tcContour;
        tcContour.reserve(contour.size());
        for (const auto& pt : contour) {
            tcContour.push_back(toTcVec2(pt));
        }
        result.push_back(std::move(tcContour));
    }

    return result;
}

} // namespace tcx
