#include "tcxCvContourFinder.h"
#include "tcxCvWrappers.h"
#include <TrussC.h>

namespace tcx {

using namespace cv;
using namespace std;

// Sort helper for contour sorting by area
struct CompareContourArea {
    CompareContourArea(const vector<double>& areaVec) : mAreaVec(areaVec) {}

    bool operator()(size_t a, size_t b) const {
        return mAreaVec[a] > mAreaVec[b];
    }

    const vector<double>& mAreaVec;
};

ContourFinder::ContourFinder()
    : autoThreshold_(true)
    , invert_(false)
    , simplify_(true)
    , thresholdValue_(128.0f)
    , useTargetColor(false)
    , contourFindingMode(RETR_EXTERNAL)
    , sortBySize_(false) {
    resetMinArea();
    resetMaxArea();
}

void ContourFinder::findContours(Mat img) {
    // Threshold the image
    if (useTargetColor) {
        Scalar offset(thresholdValue_, thresholdValue_, thresholdValue_);
        Scalar base = toCv(targetColor);
        if (trackingColorMode == TRACK_COLOR_RGB) {
            inRange(img, base - offset, base + offset, thresh);
        } else {
            if (trackingColorMode == TRACK_COLOR_H) {
                offset[1] = 255;
                offset[2] = 255;
            }
            if (trackingColorMode == TRACK_COLOR_HS) {
                offset[2] = 255;
            }
            cvtColor(img, hsvBuffer, COLOR_RGB2HSV);
            base = toCv(convertColor(targetColor, COLOR_RGB2HSV));
            Scalar lowerb = base - offset;
            Scalar upperb = base + offset;
            inRange(hsvBuffer, lowerb, upperb, thresh);
        }
    } else {
        copyGray(img, thresh);
    }
    if (autoThreshold_) {
        threshold(thresh, thresholdValue_, invert_);
    }

    // Find all contours
    vector<vector<Point>> allContours;
    int simplifyMode = simplify_ ? CHAIN_APPROX_SIMPLE : CHAIN_APPROX_NONE;
    cv::findContours(thresh, allContours, contourFindingMode, simplifyMode);

    // Filter contours by area
    bool needMinFilter = (minArea > 0);
    bool needMaxFilter = maxAreaNorm ? (maxArea < 1.0f) : (maxArea < numeric_limits<float>::infinity());
    vector<size_t> allIndices;
    vector<double> allAreas;
    vector<bool> allHoles;

    if (needMinFilter || needMaxFilter) {
        double imgArea = img.rows * img.cols;
        double imgMinArea = minAreaNorm ? (minArea * imgArea) : minArea;
        double imgMaxArea = maxAreaNorm ? (maxArea * imgArea) : maxArea;
        for (size_t i = 0; i < allContours.size(); i++) {
            double curArea = contourArea(Mat(allContours[i]), true);
            bool hole = true;
            if (curArea < 0) {
                curArea = -curArea;
                hole = false;
            }

            if ((!needMinFilter || curArea >= imgMinArea) &&
                (!needMaxFilter || curArea <= imgMaxArea)) {
                allIndices.push_back(i);
                allHoles.push_back(hole);
                allAreas.push_back(curArea);
            }
        }
    } else {
        for (size_t i = 0; i < allContours.size(); i++) {
            double curArea = contourArea(Mat(allContours[i]), true);
            allAreas.push_back(abs(curArea));
            allHoles.push_back(curArea > 0);
            allIndices.push_back(i);
        }
    }

    if (allIndices.size() > 1 && sortBySize_) {
        sort(allIndices.begin(), allIndices.end(), CompareContourArea(allAreas));
    }

    // Generate polylines and bounding boxes
    contours.clear();
    polylines.clear();
    boundingRects.clear();
    holes.clear();

    for (size_t i = 0; i < allIndices.size(); i++) {
        contours.push_back(allContours[allIndices[i]]);
        polylines.push_back(toOf(contours.back()));
        boundingRects.push_back(boundingRect(contours.back()));
        holes.push_back(allHoles[i]);
    }

    // Track bounding boxes
    tracker.track(boundingRects);
}

const vector<vector<Point>>& ContourFinder::getContours() const {
    return contours;
}

const vector<tc::Path>& ContourFinder::getPolylines() const {
    return polylines;
}

const vector<Rect>& ContourFinder::getBoundingRects() const {
    return boundingRects;
}

unsigned int ContourFinder::size() const {
    return (unsigned int)contours.size();
}

vector<Point>& ContourFinder::getContour(unsigned int i) {
    return contours[i];
}

tc::Path& ContourFinder::getPolyline(unsigned int i) {
    return polylines[i];
}

Rect ContourFinder::getBoundingRect(unsigned int i) const {
    return boundingRects[i];
}

Point2f ContourFinder::getCenter(unsigned int i) const {
    Rect box = getBoundingRect(i);
    return Point2f((float)(box.x + box.width / 2.0), (float)(box.y + box.height / 2.0));
}

Point2f ContourFinder::getCentroid(unsigned int i) const {
    Moments m = moments(contours[i]);
    if (m.m00 != 0) {
        return Point2f((float)(m.m10 / m.m00), (float)(m.m01 / m.m00));
    }
    return Point2f(0, 0);
}

Point2f ContourFinder::getAverage(unsigned int i) const {
    Scalar average = mean(contours[i]);
    return Point2f((float)average[0], (float)average[1]);
}

Vec2f ContourFinder::getBalance(unsigned int i) const {
    return Vec2f(getCentroid(i) - getCenter(i));
}

double ContourFinder::getContourArea(unsigned int i) const {
    return contourArea(contours[i]);
}

double ContourFinder::getArcLength(unsigned int i) const {
    return arcLength(contours[i], true);
}

vector<Point> ContourFinder::getConvexHull(unsigned int i) const {
    vector<Point> hull;
    convexHull(contours[i], hull);
    return hull;
}

vector<Vec4i> ContourFinder::getConvexityDefects(unsigned int i) const {
    return convexityDefects(contours[i]);
}

RotatedRect ContourFinder::getMinAreaRect(unsigned int i) const {
    return minAreaRect(contours[i]);
}

Point2f ContourFinder::getMinEnclosingCircle(unsigned int i, float& radius) const {
    Point2f center;
    minEnclosingCircle(contours[i], center, radius);
    return center;
}

RotatedRect ContourFinder::getFitEllipse(unsigned int i) const {
    if (contours[i].size() < 5) {
        return getMinAreaRect(i);
    }
    return fitEllipse(contours[i]);
}

vector<Point> ContourFinder::getFitQuad(unsigned int i) const {
    vector<Point> convexHull_ = getConvexHull(i);
    vector<Point> quad = convexHull_;

    static const unsigned int targetPoints = 4;
    static const unsigned int maxIterations = 16;
    static const double infinity = numeric_limits<double>::infinity();
    double minEpsilon = 0;
    double maxEpsilon = infinity;
    double curEpsilon = 16;

    if (quad.size() > 4) {
        for (unsigned int iter = 0; iter < maxIterations; iter++) {
            approxPolyDP(Mat(convexHull_), quad, curEpsilon, true);
            if (quad.size() == targetPoints) break;
            if (quad.size() > targetPoints) {
                minEpsilon = curEpsilon;
                if (maxEpsilon == infinity) {
                    curEpsilon = curEpsilon * 2;
                } else {
                    curEpsilon = (maxEpsilon + minEpsilon) / 2;
                }
            }
            if (quad.size() < targetPoints) {
                maxEpsilon = curEpsilon;
                curEpsilon = (maxEpsilon + minEpsilon) / 2;
            }
        }
    }

    return quad;
}

bool ContourFinder::getHole(unsigned int i) const {
    return holes[i];
}

Vec2f ContourFinder::getVelocity(unsigned int i) const {
    return tracker.getVelocity(i);
}

unsigned int ContourFinder::getLabel(unsigned int i) const {
    return tracker.getCurrentLabels()[i];
}

RectTracker& ContourFinder::getTracker() {
    return tracker;
}

double ContourFinder::pointPolygonTest(unsigned int i, Point2f point) {
    return cv::pointPolygonTest(contours[i], point, true);
}

void ContourFinder::setThreshold(float thresholdValue_) {
    this->thresholdValue_ = thresholdValue_;
}

void ContourFinder::setAutoThreshold(bool autoThreshold_) {
    this->autoThreshold_ = autoThreshold_;
}

void ContourFinder::setInvert(bool invert_) {
    this->invert_ = invert_;
}

void ContourFinder::setUseTargetColor(bool useTargetColor_) {
    this->useTargetColor = useTargetColor_;
}

void ContourFinder::setTargetColor(tc::Color targetColor_, TrackingColorMode trackingColorMode_) {
    useTargetColor = true;
    this->targetColor = targetColor_;
    this->trackingColorMode = trackingColorMode_;
}

void ContourFinder::setSimplify(bool simplify__) {
    this->simplify_ = simplify__;
}

void ContourFinder::setFindHoles(bool findHoles_) {
    if (findHoles_) {
        contourFindingMode = RETR_LIST;
    } else {
        contourFindingMode = RETR_EXTERNAL;
    }
}

void ContourFinder::setSortBySize(bool sortBySize__) {
    sortBySize_ = sortBySize__;
}

void ContourFinder::draw() const {
    for (size_t i = 0; i < polylines.size(); i++) {
        // Draw polyline
        const tc::Path& poly = polylines[i];
        if (poly.size() < 2) continue;

        for (int j = 0; j < poly.size() - 1; j++) {
            tc::drawLine(poly[j].x, poly[j].y, poly[j + 1].x, poly[j + 1].y);
        }
        // Close the contour
        tc::drawLine(poly[poly.size() - 1].x, poly[poly.size() - 1].y,
                      poly[0].x, poly[0].y);

        // Draw bounding rect
        cv::Rect r = boundingRects[i];
        tc::drawRect((float)r.x, (float)r.y, (float)r.width, (float)r.height);
    }
}

// Area setters
void ContourFinder::resetMinArea() {
    setMinArea(0);
}
void ContourFinder::resetMaxArea() {
    setMaxArea(numeric_limits<float>::infinity());
}
void ContourFinder::setMinArea(float minArea_) {
    this->minArea = minArea_;
    minAreaNorm = false;
}
void ContourFinder::setMaxArea(float maxArea_) {
    this->maxArea = maxArea_;
    maxAreaNorm = false;
}
void ContourFinder::setMinAreaRadius(float minAreaRadius) {
    minArea = 3.14159265358979f * minAreaRadius * minAreaRadius;
    minAreaNorm = false;
}
void ContourFinder::setMaxAreaRadius(float maxAreaRadius) {
    maxArea = 3.14159265358979f * maxAreaRadius * maxAreaRadius;
    maxAreaNorm = false;
}
void ContourFinder::setMinAreaNorm(float minAreaNorm_) {
    minArea = minAreaNorm_;
    this->minAreaNorm = true;
}
void ContourFinder::setMaxAreaNorm(float maxAreaNorm_) {
    maxArea = maxAreaNorm_;
    this->maxAreaNorm = true;
}

} // namespace tcx
