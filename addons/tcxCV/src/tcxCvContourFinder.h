#pragma once

// ======================================================================
// tcxCvContourFinder.h - Contour finding and tracking
// ======================================================================
//
// Finds contours in an image, with automatic thresholding and color tracking.
// Tracks contours across frames using RectTracker.
//
// Usage:
//   ContourFinder finder;
//   finder.setMinAreaRadius(10);
//   finder.setMaxAreaRadius(200);
//   finder.findContours(img);
//   for (unsigned int i = 0; i < finder.size(); i++) {
//       tc::Path& poly = finder.getPolyline(i);
//       cv::Rect box = finder.getBoundingRect(i);
//   }
//

#include <TrussC.h>
#include <opencv2/opencv.hpp>
#include "tcxCvUtilities.h"
#include "tcxCvWrappers.h"
#include "tcxCvTracker.h"

namespace tcx {

enum TrackingColorMode {
    TRACK_COLOR_RGB,
    TRACK_COLOR_HSV,
    TRACK_COLOR_H,
    TRACK_COLOR_HS
};

class ContourFinder {
public:
    ContourFinder();

    template <class T>
    void findContours(T& img) {
        findContours(toCv(img));
    }
    void findContours(cv::Mat img);

    const std::vector<std::vector<cv::Point>>& getContours() const;
    const std::vector<tc::Path>& getPolylines() const;
    const std::vector<cv::Rect>& getBoundingRects() const;

    unsigned int size() const;
    std::vector<cv::Point>& getContour(unsigned int i);
    tc::Path& getPolyline(unsigned int i);

    cv::Rect getBoundingRect(unsigned int i) const;
    cv::Point2f getCenter(unsigned int i) const;       // center of bounding box
    cv::Point2f getCentroid(unsigned int i) const;     // center of mass
    cv::Point2f getAverage(unsigned int i) const;      // average of contour vertices
    cv::Vec2f getBalance(unsigned int i) const;        // centroid - center
    double getContourArea(unsigned int i) const;
    double getArcLength(unsigned int i) const;
    std::vector<cv::Point> getConvexHull(unsigned int i) const;
    std::vector<cv::Vec4i> getConvexityDefects(unsigned int i) const;
    cv::RotatedRect getMinAreaRect(unsigned int i) const;
    cv::Point2f getMinEnclosingCircle(unsigned int i, float& radius) const;
    cv::RotatedRect getFitEllipse(unsigned int i) const;
    std::vector<cv::Point> getFitQuad(unsigned int i) const;
    bool getHole(unsigned int i) const;
    cv::Vec2f getVelocity(unsigned int i) const;

    RectTracker& getTracker();
    unsigned int getLabel(unsigned int i) const;

    // Point-in-contour test (positive = inside)
    double pointPolygonTest(unsigned int i, cv::Point2f point);

    void setThreshold(float thresholdValue);
    void setAutoThreshold(bool autoThreshold);
    void setInvert(bool invert);
    void setUseTargetColor(bool useTargetColor);
    void setTargetColor(tc::Color targetColor,
                        TrackingColorMode trackingColorMode = TRACK_COLOR_RGB);
    void setFindHoles(bool findHoles);
    void setSortBySize(bool sortBySize);

    void resetMinArea();
    void resetMaxArea();
    void setMinArea(float minArea);
    void setMaxArea(float maxArea);
    void setMinAreaRadius(float minAreaRadius);
    void setMaxAreaRadius(float maxAreaRadius);
    void setMinAreaNorm(float minAreaNorm);
    void setMaxAreaNorm(float maxAreaNorm);

    void setSimplify(bool simplify);

    void draw() const;

protected:
    cv::Mat hsvBuffer, thresh;
    bool autoThreshold_, invert_, simplify_;
    float thresholdValue_;

    bool useTargetColor;
    TrackingColorMode trackingColorMode;
    tc::Color targetColor;

    float minArea, maxArea;
    bool minAreaNorm, maxAreaNorm;

    std::vector<std::vector<cv::Point>> contours;
    std::vector<tc::Path> polylines;

    RectTracker tracker;
    std::vector<cv::Rect> boundingRects;
    std::vector<bool> holes;

    int contourFindingMode;
    bool sortBySize_;
};

} // namespace tcx
