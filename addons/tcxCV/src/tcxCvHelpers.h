#pragma once

// ======================================================================
// tcxCvHelpers.h - High-level helper functions
// ======================================================================
//
// Provides drawing utilities, matrix helpers, contour simplification,
// and image thinning algorithms.
//

#include <TrussC.h>
#include <opencv2/opencv.hpp>
#include "tcxCvUtilities.h"

namespace tcx {

// ======================================================================
// Drawing helpers
// ======================================================================

// Draw cv::Mat as a texture at (x, y)
void drawMat(const cv::Mat& mat, float x, float y);

// Draw cv::Mat as a texture at (x, y) with custom size
void drawMat(const cv::Mat& mat, float x, float y, float width, float height);

// ======================================================================
// Matrix helpers
// ======================================================================

// Build a 4x4 matrix from OpenCV rotation and translation
tc::Mat4 makeMatrix(cv::Mat rotation, cv::Mat translation);

// ======================================================================
// Array search helpers
// ======================================================================

// Find first occurrence of target value in column array
int findFirst(const cv::Mat& arr, unsigned char target);

// Find last occurrence of target value in column array
int findLast(const cv::Mat& arr, unsigned char target);

// ======================================================================
// Line / contour analysis
// ======================================================================

// Weighted average angle of a set of lines (weight = length^2)
float weightedAverageAngle(const std::vector<cv::Vec4i>& lines);

// Simplify convex hull to a target number of points
std::vector<cv::Point2f> getConvexPolygon(
    const std::vector<cv::Point2f>& convexHull, int targetPoints);

// ======================================================================
// Thinning (Zhang-Suen algorithm)
// ======================================================================

// Single iteration of Zhang-Suen thinning.
// im: Binary image with range [0,1]. iter: 0=even, 1=odd.
void thinningIteration(cv::Mat& im, int iter, cv::Mat& marker);

// Full Zhang-Suen thinning of a binary image.
inline void thin(cv::Mat& im) {
    im /= 255;
    cv::Mat prev = cv::Mat::zeros(im.size(), CV_8UC1);
    cv::Mat diff;
    do {
        cv::Mat marker = cv::Mat::zeros(im.size(), CV_8UC1);
        thinningIteration(im, 0, marker);
        thinningIteration(im, 1, marker);
        cv::absdiff(im, prev, diff);
        im.copyTo(prev);
    } while (cv::countNonZero(diff) > 0);
    im *= 255;
}

} // namespace tcx
