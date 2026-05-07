#include "tcxCvObjectFinder.h"
#include "tcxCvWrappers.h"
#include <TrussC.h>

namespace tcx {

using namespace cv;
using namespace std;

ObjectFinder::ObjectFinder()
    : rescale_(1.0f)
    , multiScaleFactor_(1.1f)
    , minNeighbors_(3)
    , minSizeScale_(0.0f)
    , maxSizeScale_(1.0f)
    , useHistogramEqualization_(true)
    , cannyPruning_(false)
    , findBiggestObject_(false) {
}

void ObjectFinder::setup(const string& cascadeFilename) {
    if (!classifier.load(cascadeFilename)) {
        tc::logError() << "ObjectFinder::setup() - Couldn't load cascade file: " << cascadeFilename;
    }
}

void ObjectFinder::update(Mat img) {
    Mat gray_;
    if (getChannels(img) == 1) {
        gray_ = img;
    } else {
        copyGray(img, gray_);
    }
    resize(gray_, graySmall, rescale_, rescale_);
    Mat graySmallMat = toCv(graySmall);

    if (useHistogramEqualization_) {
        equalizeHist(graySmallMat, graySmallMat);
    }

    Size minSize, maxSize;
    float minSide = (float)std::min(graySmallMat.rows, graySmallMat.cols);
    if (minSizeScale_ > 0) {
        int side = (int)(minSizeScale_ * minSide);
        minSize = Size(side, side);
    }
    if (maxSizeScale_ < 1) {
        int side = (int)(maxSizeScale_ * minSide);
        maxSize = Size(side, side);
    }

    classifier.detectMultiScale(graySmallMat, objects,
        multiScaleFactor_, minNeighbors_,
        (cannyPruning_ ? CASCADE_DO_CANNY_PRUNING : 0) |
        (findBiggestObject_ ? CASCADE_FIND_BIGGEST_OBJECT | CASCADE_DO_ROUGH_SEARCH : 0),
        minSize, maxSize);

    // Scale back to original image coordinates
    for (size_t i = 0; i < objects.size(); i++) {
        Rect& rect = objects[i];
        rect.x = (int)(rect.x / rescale_);
        rect.y = (int)(rect.y / rescale_);
        rect.width = (int)(rect.width / rescale_);
        rect.height = (int)(rect.height / rescale_);
    }

    tracker.track(objects);
}

unsigned int ObjectFinder::size() const {
    return (unsigned int)objects.size();
}

tc::Rect ObjectFinder::getObject(unsigned int i) const {
    return toOf(objects[i]);
}

tc::Rect ObjectFinder::getObjectSmoothed(unsigned int i) const {
    return toOf(tracker.getSmoothed(getLabel(i)));
}

cv::Vec2f ObjectFinder::getVelocity(unsigned int i) const {
    return tracker.getVelocity(i);
}

unsigned int ObjectFinder::getLabel(unsigned int i) const {
    return tracker.getCurrentLabels()[i];
}

RectTracker& ObjectFinder::getTracker() {
    return tracker;
}

void ObjectFinder::draw() const {
    for (unsigned int i = 0; i < size(); i++) {
        tc::Rect obj = getObject(i);
        tc::drawRect(obj.x, obj.y, obj.width, obj.height);
    }
}

void ObjectFinder::setPreset(Preset preset) {
    if (preset == Fast) {
        setRescale(0.25f);
        setMinNeighbors(2);
        setMultiScaleFactor(1.2f);
        setMinSizeScale(0.25f);
        setMaxSizeScale(0.75f);
        setCannyPruning(true);
        setFindBiggestObject(false);
    } else if (preset == Accurate) {
        setRescale(0.5f);
        setMinNeighbors(6);
        setMultiScaleFactor(1.02f);
        setMinSizeScale(0.1f);
        setMaxSizeScale(1.0f);
        setCannyPruning(true);
        setFindBiggestObject(false);
    } else if (preset == Sensitive) {
        setRescale(0.5f);
        setMinNeighbors(1);
        setMultiScaleFactor(1.02f);
        setMinSizeScale(0.1f);
        setMaxSizeScale(1.0f);
        setCannyPruning(false);
        setFindBiggestObject(false);
    }
}

// Getters and setters
void ObjectFinder::setRescale(float rescale__) { this->rescale_ = rescale__; }
void ObjectFinder::setMinNeighbors(int minNeighbors__) { this->minNeighbors_ = minNeighbors__; }
void ObjectFinder::setMultiScaleFactor(float multiScaleFactor__) { this->multiScaleFactor_ = multiScaleFactor__; }
void ObjectFinder::setCannyPruning(bool cannyPruning__) { this->cannyPruning_ = cannyPruning__; }
void ObjectFinder::setFindBiggestObject(bool findBiggestObject__) { this->findBiggestObject_ = findBiggestObject__; }
void ObjectFinder::setUseHistogramEqualization(bool useHistogramEqualization__) { this->useHistogramEqualization_ = useHistogramEqualization__; }
void ObjectFinder::setMinSizeScale(float minSizeScale__) { this->minSizeScale_ = minSizeScale__; }
void ObjectFinder::setMaxSizeScale(float maxSizeScale__) { this->maxSizeScale_ = maxSizeScale__; }

float ObjectFinder::getRescale() const { return rescale_; }
int ObjectFinder::getMinNeighbors() const { return minNeighbors_; }
float ObjectFinder::getMultiScaleFactor() const { return multiScaleFactor_; }
bool ObjectFinder::getCannyPruning() const { return cannyPruning_; }
bool ObjectFinder::getFindBiggestObject() const { return findBiggestObject_; }
bool ObjectFinder::getUseHistogramEqualization() const { return useHistogramEqualization_; }
float ObjectFinder::getMinSizeScale() const { return minSizeScale_; }
float ObjectFinder::getMaxSizeScale() const { return maxSizeScale_; }

} // namespace tcx
