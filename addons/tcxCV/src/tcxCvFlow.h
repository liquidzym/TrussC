#pragma once

// ======================================================================
// tcxCvFlow.h - Optical flow (PyrLK and Farneback)
// ======================================================================
//
// Two optical flow implementations:
//   FlowPyrLK   - Sparse Lucas-Kanade feature tracking
//   FlowFarneback - Dense Farneback flow field
//
// Usage (PyrLK):
//   FlowPyrLK flow;
//   flow.calcOpticalFlow(prevImg, curImg);
//   vector<tc::Vec2> motion = flow.getMotion();
//
// Usage (Farneback):
//   FlowFarneback flow;
//   flow.calcOpticalFlow(prevImg, curImg);
//   tc::Vec2 avg = flow.getAverageFlow();
//

#include <TrussC.h>
#include <opencv2/opencv.hpp>
#include "tcxCvUtilities.h"
#include "tcxCvWrappers.h"

namespace tcx {

// ======================================================================
// Flow - base class
// ======================================================================

class Flow {
public:
    Flow();
    virtual ~Flow();

    // Calculate flow from two consecutive images
    template <class T>
    void calcOpticalFlow(const T& lastImage, const T& currentImage) {
        calcOpticalFlow(toCv(lastImage), toCv(currentImage));
    }
    void calcOpticalFlow(cv::Mat lastImage, cv::Mat currentImage);

    // Calculate flow from a single image (stores previous internally)
    template <class T>
    void calcOpticalFlow(const T& currentImage) {
        calcOpticalFlow(toCv(currentImage));
    }
    void calcOpticalFlow(cv::Mat nextImage);

    void draw();
    void draw(float x, float y);
    void draw(float x, float y, float width, float height);
    void draw(tc::Rect rect);

    int getWidth();
    int getHeight();

    virtual void resetFlow();

private:
    cv::Mat last, curr;

protected:
    bool hasFlow;

    virtual void calcFlow(cv::Mat prev, cv::Mat next) = 0;
    virtual void drawFlow(tc::Rect r) = 0;
};

// ======================================================================
// FlowPyrLK - Sparse Lucas-Kanade feature tracking
// ======================================================================

class FlowPyrLK : public Flow {
public:
    FlowPyrLK();
    virtual ~FlowPyrLK();

    void setMinDistance(int minDistance);
    void setWindowSize(int winsize);
    void setMaxLevel(int maxLevel);
    void setMaxFeatures(int maxFeatures);
    void setQualityLevel(float qualityLevel);

    std::vector<tc::Vec2> getCurrent();
    std::vector<tc::Vec2> getMotion();

    void resetFeaturesToTrack();
    void setFeaturesToTrack(const std::vector<tc::Vec2>& features);
    void setFeaturesToTrack(const std::vector<cv::Point2f>& features);
    void resetFlow();

protected:
    void drawFlow(tc::Rect r);
    void calcFlow(cv::Mat prev, cv::Mat next);
    void calcFeaturesToTrack(std::vector<cv::Point2f>& features, cv::Mat next);

    std::vector<cv::Point2f> prevPts, nextPts;

    int windowSize;
    int maxLevel;
    int maxFeatures;
    float qualityLevel;
    int minDistance;

    bool calcFeaturesNextFrame;

    std::vector<cv::Mat> pyramid;
    std::vector<cv::Mat> prevPyramid;
    std::vector<uchar> status;
    std::vector<float> err;
};

// ======================================================================
// FlowFarneback - Dense Farneback optical flow
// ======================================================================

class FlowFarneback : public Flow {
public:
    FlowFarneback();
    virtual ~FlowFarneback();

    void setPyramidScale(float scale);
    void setNumLevels(int levels);
    void setWindowSize(int winsize);
    void setNumIterations(int iterations);
    void setPolyN(int polyN);
    void setPolySigma(float polySigma);
    void setUseGaussian(bool gaussian);

    cv::Mat& getFlow();
    tc::Vec2 getTotalFlow();
    tc::Vec2 getAverageFlow();
    tc::Vec2 getFlowOffset(int x, int y);
    tc::Vec2 getFlowPosition(int x, int y);
    tc::Vec2 getTotalFlowInRegion(tc::Rect region);
    tc::Vec2 getAverageFlowInRegion(tc::Rect region);

    void resetFlow();

protected:
    cv::Mat flow;
    int numLevels;
    int windowSize;
    int numIterations;
    int polyN;
    float pyramidScale;
    float polySigma;
    bool farnebackGaussian;

    void drawFlow(tc::Rect rect);
    void calcFlow(cv::Mat prev, cv::Mat next);
};

} // namespace tcx
