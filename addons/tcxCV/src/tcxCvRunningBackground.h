#pragma once

// ======================================================================
// tcxCvRunningBackground.h - Background subtraction via running average
// ======================================================================
//
// Detects foreground objects by accumulating a background model and
// subtracting it from each new frame.
//
// Usage:
//   RunningBackground bg;
//   bg.setLearningTime(900); // ~30s at 30fps
//   bg.setThresholdValue(26);
//   bg.update(frame, thresholded);
//   cv::Mat& background = bg.getBackground();
//

#include <TrussC.h>
#include <opencv2/opencv.hpp>
#include "tcxCvUtilities.h"
#include <opencv2/opencv.hpp>

namespace tcx {

class RunningBackground {
public:
    enum DifferenceMode { ABSDIFF, BRIGHTER, DARKER };

    RunningBackground();

    template <class F, class T>
    void update(F& frame, T& thresholded) {
        imitate(thresholded, frame, CV_8UC1);
        cv::Mat frameMat = toCv(frame);
        cv::Mat thresholdedMat = toCv(thresholded);
        update(frameMat, thresholdedMat);
    }

    void update(cv::Mat frame, cv::Mat& thresholded);

    cv::Mat& getBackground();
    cv::Mat& getForeground();
    float getPresence() const;

    void setThresholdValue(unsigned int thresholdValue);
    void setLearningRate(double learningRate);
    void setLearningTime(double learningTime);
    void setIgnoreForeground(bool ignoreForeground);
    void setDifferenceMode(DifferenceMode differenceMode);
    void reset();

protected:
    cv::Mat accumulator, background, foreground, foregroundGray;
    double learningRate, learningTime;
    unsigned int thresholdValue;
    bool useLearningTime, needToReset, ignoreForeground;
    DifferenceMode differenceMode;
};

} // namespace tcx
