#include "tcxCvRunningBackground.h"
#include "tcxCvUtilities.h"
#include "tcxCvWrappers.h"

namespace tcx {

using namespace cv;
using namespace std;

RunningBackground::RunningBackground()
    : learningRate(0.0001)
    , learningTime(900.0)
    , thresholdValue(26)
    , useLearningTime(false)
    , needToReset(true)
    , ignoreForeground(false)
    , differenceMode(ABSDIFF) {
}

void RunningBackground::update(Mat frame, Mat& thresholded) {
    if (needToReset || accumulator.empty()) {
        needToReset = false;
        frame.copyTo(accumulator);
        frame.copyTo(background);
    }

    // Learn background
    if (ignoreForeground) {
        // Only update background where foreground is not present
        Mat notForeground;
        bitwise_not(thresholded, notForeground);
        Mat backgroundUpdate;
        if (useLearningTime) {
            addWeighted(accumulator, learningTime, frame, 1.0, 0.0, backgroundUpdate);
        } else {
            addWeighted(accumulator, 1.0 - learningRate, frame, learningRate, 0.0, backgroundUpdate);
        }
        backgroundUpdate.copyTo(accumulator, notForeground);
    } else {
        if (useLearningTime) {
            addWeighted(accumulator, learningTime, frame, 1.0, 0.0, accumulator);
        } else {
            accumulateWeighted(frame, accumulator, learningRate);
        }
    }

    accumulator.copyTo(background);

    // Compute difference
    if (differenceMode == ABSDIFF) {
        absdiff(frame, background, foreground);
    } else {
        foreground = frame - background;
        if (differenceMode == DARKER) {
            foreground = -foreground;
        }
    }

    // Threshold
    if (foreground.channels() == 1) {
        foregroundGray = foreground;
    } else {
        copyGray(foreground, foregroundGray);
    }
    threshold(foregroundGray, thresholded, (double)thresholdValue, 255.0, THRESH_BINARY);
}

Mat& RunningBackground::getBackground() {
    return background;
}

Mat& RunningBackground::getForeground() {
    return foregroundGray;
}

float RunningBackground::getPresence() const {
    Scalar meanVal = mean(foregroundGray);
    return (float)(meanVal[0] / 255.0);
}

void RunningBackground::setThresholdValue(unsigned int thresholdValue_) {
    this->thresholdValue = thresholdValue_;
}

void RunningBackground::setLearningRate(double learningRate_) {
    this->learningRate = learningRate_;
    useLearningTime = false;
}

void RunningBackground::setLearningTime(double learningTime_) {
    this->learningTime = 1.0 / learningTime_;
    useLearningTime = true;
}

void RunningBackground::setIgnoreForeground(bool ignoreForeground_) {
    this->ignoreForeground = ignoreForeground_;
}

void RunningBackground::setDifferenceMode(DifferenceMode differenceMode_) {
    this->differenceMode = differenceMode_;
}

void RunningBackground::reset() {
    needToReset = true;
}

} // namespace tcx
