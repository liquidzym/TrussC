#include "tcxCvFlow.h"
#include <TrussC.h>

namespace tcx {

using namespace cv;
using namespace std;

// ======================================================================
// Flow base class
// ======================================================================

Flow::Flow()
    : hasFlow(false) {
}

Flow::~Flow() {
}

void Flow::calcOpticalFlow(Mat lastImage, Mat currentImage) {
    if (lastImage.channels() == 1 && currentImage.channels() == 1) {
        calcFlow(lastImage, currentImage);
    } else {
        copyGray(lastImage, last);
        copyGray(currentImage, curr);
        calcFlow(last, curr);
    }
    hasFlow = true;
}

void Flow::calcOpticalFlow(Mat nextImage) {
    copyGray(nextImage, curr);
    if (last.size == curr.size) {
        calcFlow(last, curr);
        hasFlow = true;
    }
    swap(curr, last);
}

void Flow::draw() {
    if (hasFlow) {
        drawFlow(tc::Rect(0, 0, (float)getWidth(), (float)getHeight()));
    }
}

void Flow::draw(float x, float y) {
    if (hasFlow) {
        drawFlow(tc::Rect(x, y, (float)getWidth(), (float)getHeight()));
    }
}

void Flow::draw(float x, float y, float width, float height) {
    if (hasFlow) {
        drawFlow(tc::Rect(x, y, width, height));
    }
}

void Flow::draw(tc::Rect rect) {
    if (hasFlow) {
        drawFlow(rect);
    }
}

int Flow::getWidth() {
    return curr.cols;
}

int Flow::getHeight() {
    return curr.rows;
}

void Flow::resetFlow() {
    last = Mat();
    curr = Mat();
    hasFlow = false;
}

// ======================================================================
// FlowPyrLK
// ======================================================================

FlowPyrLK::FlowPyrLK()
    : windowSize(32)
    , maxLevel(3)
    , maxFeatures(200)
    , qualityLevel(0.01f)
    , minDistance(4)
    , calcFeaturesNextFrame(true) {
}

FlowPyrLK::~FlowPyrLK() {
}

void FlowPyrLK::setWindowSize(int winsize) { windowSize = winsize; }
void FlowPyrLK::setMaxLevel(int ml) { maxLevel = ml; }
void FlowPyrLK::setMaxFeatures(int mf) { maxFeatures = mf; }
void FlowPyrLK::setQualityLevel(float ql) { qualityLevel = ql; }
void FlowPyrLK::setMinDistance(int md) { minDistance = md; }

void FlowPyrLK::calcFlow(Mat prev, Mat next) {
    if (!nextPts.empty() || calcFeaturesNextFrame) {
        if (calcFeaturesNextFrame) {
            calcFeaturesToTrack(prevPts, next);
            if (prevPts.empty()) {
                nextPts.clear();
                return;
            }
            calcFeaturesNextFrame = false;
        } else {
            swap(prevPts, nextPts);
        }
        nextPts.clear();

        // Use pyramid-based tracking (OpenCV 3.x+/4.x API)
        if (prevPyramid.empty()) {
            buildOpticalFlowPyramid(prev, prevPyramid, Size(windowSize, windowSize), 10);
        }
        buildOpticalFlowPyramid(next, pyramid, Size(windowSize, windowSize), 10);
        calcOpticalFlowPyrLK(prevPyramid, pyramid, prevPts, nextPts,
                             status, err, Size(windowSize, windowSize), maxLevel);
        prevPyramid = pyramid;
        pyramid.clear();

        status.resize(nextPts.size(), 0);
    } else {
        calcFeaturesToTrack(nextPts, next);
    }
}

void FlowPyrLK::calcFeaturesToTrack(vector<Point2f>& features, Mat next_) {
    goodFeaturesToTrack(next_, features, maxFeatures, qualityLevel, (double)minDistance);
}

void FlowPyrLK::resetFeaturesToTrack() {
    calcFeaturesNextFrame = true;
}

void FlowPyrLK::setFeaturesToTrack(const vector<tc::Vec2>& features) {
    nextPts.resize(features.size());
    for (size_t i = 0; i < features.size(); i++) {
        nextPts[i] = toCv(features[i]);
    }
    calcFeaturesNextFrame = false;
}

void FlowPyrLK::setFeaturesToTrack(const vector<cv::Point2f>& features) {
    nextPts = features;
    calcFeaturesNextFrame = false;
}

vector<tc::Vec2> FlowPyrLK::getCurrent() {
    vector<tc::Vec2> ret;
    for (size_t i = 0; i < nextPts.size(); i++) {
        if (status[i]) {
            ret.push_back(toOf(nextPts[i]));
        }
    }
    return ret;
}

vector<tc::Vec2> FlowPyrLK::getMotion() {
    vector<tc::Vec2> ret;
    for (size_t i = 0; i < prevPts.size(); i++) {
        if (i < status.size() && status[i]) {
            ret.push_back(toOf(nextPts[i]) - toOf(prevPts[i]));
        }
    }
    return ret;
}

void FlowPyrLK::drawFlow(tc::Rect rect) {
    tc::Vec2 offset(rect.x, rect.y);
    tc::Vec2 scale(rect.width / getWidth(), rect.height / getHeight());
    for (size_t i = 0; i < prevPts.size(); i++) {
        if (i < status.size() && status[i]) {
            tc::Vec2 a = toOf(prevPts[i]) * scale + offset;
            tc::Vec2 b = toOf(nextPts[i]) * scale + offset;
            tc::drawLine(a.x, a.y, b.x, b.y);
        }
    }
}

void FlowPyrLK::resetFlow() {
    Flow::resetFlow();
    resetFeaturesToTrack();
    prevPts.clear();
}

// ======================================================================
// FlowFarneback
// ======================================================================

FlowFarneback::FlowFarneback()
    : pyramidScale(0.5f)
    , numLevels(4)
    , windowSize(8)
    , numIterations(2)
    , polyN(7)
    , polySigma(1.5f)
    , farnebackGaussian(false) {
}

FlowFarneback::~FlowFarneback() {
}

void FlowFarneback::setPyramidScale(float scale) { pyramidScale = scale; }
void FlowFarneback::setNumLevels(int levels) { numLevels = levels; }
void FlowFarneback::setWindowSize(int winsize) { windowSize = winsize; }
void FlowFarneback::setNumIterations(int iterations) { numIterations = iterations; }
void FlowFarneback::setPolyN(int n) { polyN = n; }
void FlowFarneback::setPolySigma(float sigma) { polySigma = sigma; }
void FlowFarneback::setUseGaussian(bool gaussian) { farnebackGaussian = gaussian; }

void FlowFarneback::resetFlow() {
    Flow::resetFlow();
    flow.setTo(0);
}

void FlowFarneback::calcFlow(Mat prev, Mat next) {
    int flags = 0;
    if (hasFlow) {
        flags |= OPTFLOW_USE_INITIAL_FLOW;
    }
    if (farnebackGaussian) {
        flags |= OPTFLOW_FARNEBACK_GAUSSIAN;
    }

    calcOpticalFlowFarneback(prev, next, flow,
                             pyramidScale, numLevels, windowSize,
                             numIterations, polyN, polySigma, flags);
}

Mat& FlowFarneback::getFlow() {
    if (!hasFlow) {
        flow = Mat::zeros(1, 1, CV_32FC2);
    }
    return flow;
}

tc::Vec2 FlowFarneback::getFlowOffset(int x, int y) {
    if (!hasFlow) return tc::Vec2(0, 0);
    const Vec2f& vec = flow.at<Vec2f>(y, x);
    return tc::Vec2(vec[0], vec[1]);
}

tc::Vec2 FlowFarneback::getFlowPosition(int x, int y) {
    if (!hasFlow) return tc::Vec2(0, 0);
    const Vec2f& vec = flow.at<Vec2f>(y, x);
    return tc::Vec2((float)x + vec[0], (float)y + vec[1]);
}

tc::Vec2 FlowFarneback::getTotalFlow() {
    return getTotalFlowInRegion(tc::Rect(0, 0, (float)flow.cols, (float)flow.rows));
}

tc::Vec2 FlowFarneback::getAverageFlow() {
    return getAverageFlowInRegion(tc::Rect(0, 0, (float)flow.cols, (float)flow.rows));
}

tc::Vec2 FlowFarneback::getAverageFlowInRegion(tc::Rect region) {
    float area = region.width * region.height;
    if (area > 0) {
        tc::Vec2 total = getTotalFlowInRegion(region);
        return tc::Vec2(total.x / area, total.y / area);
    }
    return tc::Vec2(0, 0);
}

tc::Vec2 FlowFarneback::getTotalFlowInRegion(tc::Rect region) {
    if (!hasFlow) return tc::Vec2(0, 0);
    const Scalar& sc = sum(flow(toCv(region)));
    return tc::Vec2((float)sc[0], (float)sc[1]);
}

void FlowFarneback::drawFlow(tc::Rect rect) {
    if (!hasFlow) return;

    tc::Vec2 offset(rect.x, rect.y);
    tc::Vec2 scale(rect.width / flow.cols, rect.height / flow.rows);
    int stepSize = 4; // TODO: make configurable

    for (int y = 0; y < flow.rows; y += stepSize) {
        for (int x = 0; x < flow.cols; x += stepSize) {
            tc::Vec2 cur((float)x * scale.x + offset.x,
                         (float)y * scale.y + offset.y);
            tc::Vec2 pos = getFlowPosition(x, y);
            tc::Vec2 dest(pos.x * scale.x + offset.x,
                          pos.y * scale.y + offset.y);
            tc::drawLine(cur.x, cur.y, dest.x, dest.y);
        }
    }
}

} // namespace tcx
