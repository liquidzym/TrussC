#pragma once

// ======================================================================
// tcxCvObjectFinder.h - Cascade classifier object detection
// ======================================================================
//
// Object detection using OpenCV CascadeClassifier (e.g., face detection).
//
// Usage:
//   ObjectFinder finder;
//   finder.setup("haarcascade_frontalface_default.xml");
//   finder.setPreset(ObjectFinder::Fast);
//   finder.update(img);
//   for (unsigned int i = 0; i < finder.size(); i++) {
//       tc::Rect obj = finder.getObject(i);
//   }
//

#include <TrussC.h>
#include <opencv2/opencv.hpp>
#include "tcxCvUtilities.h"
#include "tcxCvTracker.h"

namespace tcx {

class ObjectFinder {
public:
    ObjectFinder();
    void setup(const std::string& cascadeFilename);

    template <class T>
    void update(T& img) {
        update(toCv(img));
    }
    void update(cv::Mat img);

    unsigned int size() const;
    tc::Rect getObject(unsigned int i) const;
    tc::Rect getObjectSmoothed(unsigned int i) const;
    RectTracker& getTracker();
    unsigned int getLabel(unsigned int i) const;
    cv::Vec2f getVelocity(unsigned int i) const;
    void draw() const;

    enum Preset { Fast, Accurate, Sensitive };
    void setPreset(Preset preset);

    void setRescale(float rescale);
    void setMinNeighbors(int minNeighbors);
    void setMultiScaleFactor(float multiScaleFactor);
    void setCannyPruning(bool cannyPruning);
    void setFindBiggestObject(bool findBiggestObject);
    void setUseHistogramEqualization(bool useHistogramEqualization);
    void setMinSizeScale(float minSizeScale);
    void setMaxSizeScale(float maxSizeScale);

    float getRescale() const;
    int getMinNeighbors() const;
    float getMultiScaleFactor() const;
    bool getCannyPruning() const;
    bool getFindBiggestObject() const;
    bool getUseHistogramEqualization() const;
    float getMinSizeScale() const;
    float getMaxSizeScale() const;

protected:
    float rescale_, multiScaleFactor_;
    int minNeighbors_;
    bool useHistogramEqualization_, cannyPruning_, findBiggestObject_;
    float minSizeScale_, maxSizeScale_;
    cv::Mat gray, graySmall;
    cv::CascadeClassifier classifier;
    std::vector<cv::Rect> objects;
    RectTracker tracker;
};

} // namespace tcx
