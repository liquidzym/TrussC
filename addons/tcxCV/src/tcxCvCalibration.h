#pragma once

// ======================================================================
// tcxCvCalibration.h - Camera calibration and undistortion
// ======================================================================
//
// Handles per-camera intrinsic calibration and undistortion.
// Given chessboard images, calculates camera intrinsics.
//
// Usage:
//   Calibration cal;
//   cal.setPatternSize(10, 7);          // chessboard inner corners
//   cal.setSquareSize(2.5f);             // square size in world units
//   cal.add(boardImage);                 // add calibration images
//   cal.calibrate();                     // compute intrinsics
//   cal.save("camera.yml");             // save calibration
//   cal.undistort(distortedImage);       // undistort in-place
//

#include <TrussC.h>
#include <opencv2/opencv.hpp>
#include "tcxCvUtilities.h"
#include "tcxCvHelpers.h"

namespace tcx {

// ======================================================================
// Intrinsics - camera intrinsic parameters
// ======================================================================

class Intrinsics {
public:
    void setup(float focalLengthMm, cv::Size imageSizePx, cv::Size2f sensorSizeMm,
               cv::Point2d principalPointPct = cv::Point2d(0.5, 0.5));
    void setup(cv::Mat cameraMatrix, cv::Size imageSizePx,
               cv::Size2f sensorSizeMm = cv::Size2f(0, 0));

    void setImageSize(cv::Size imgSize);
    cv::Mat getCameraMatrix() const;
    cv::Size getImageSize() const;
    cv::Size2f getSensorSize() const;
    cv::Point2d getFov() const;
    double getFocalLength() const;
    double getAspectRatio() const;
    cv::Point2d getPrincipalPoint() const;

protected:
    void updateValues();
    cv::Mat cameraMatrix;
    cv::Size imageSize;
    cv::Size2f sensorSize;
    cv::Point2d fov;
    double focalLength, aspectRatio;
    cv::Point2d principalPoint;
};

// ======================================================================
// CalibrationPattern
// ======================================================================

enum CalibrationPattern { CHESSBOARD, CIRCLES_GRID, ASYMMETRIC_CIRCLES_GRID };

// ======================================================================
// Calibration - camera calibration class
// ======================================================================

class Calibration {
public:
    Calibration();

    void save(const std::string& filename) const;
    void load(const std::string& filename);
    void reset();

    void setPatternType(CalibrationPattern patternType);
    void setPatternSize(int xCount, int yCount);
    void setSquareSize(float squareSize);
    // Set the pixel size of your smallest square (default 11)
    void setSubpixelSize(int subpixelSize);

    bool add(cv::Mat img);
    bool clean(float minReprojectionError = 2.0f);
    bool calibrate();
    bool calibrateFromDirectory(const std::string& directory);
    bool findBoard(cv::Mat img, std::vector<cv::Point2f>& pointBuf, bool refine = true);

    void setIntrinsics(Intrinsics& distortedIntrinsics);
    void setDistortionCoefficients(float k1, float k2, float p1, float p2,
                                   float k3 = 0, float k4 = 0,
                                   float k5 = 0, float k6 = 0);

    void undistort(cv::Mat img, int interpolationMode = cv::INTER_LINEAR);
    void undistort(cv::Mat src, cv::Mat dst, int interpolationMode = cv::INTER_LINEAR);

    tc::Vec2 undistort(tc::Vec2& src) const;
    void undistort(std::vector<tc::Vec2>& src, std::vector<tc::Vec2>& dst) const;

    bool getTransformation(Calibration& dst, cv::Mat& rotation, cv::Mat& translation);

    float getReprojectionError() const;
    float getReprojectionError(int i) const;

    const Intrinsics& getDistortedIntrinsics() const;
    const Intrinsics& getUndistortedIntrinsics() const;
    cv::Mat getDistCoeffs() const;

    // If you want a wider fov, set fillFrame=false before load() or calibrate()
    void setFillFrame(bool fillFrame);

    size_t size() const;
    cv::Size getPatternSize() const;
    float getSquareSize() const;
    static std::vector<cv::Point3f> createObjectPoints(cv::Size patternSize,
                                                       float squareSize,
                                                       CalibrationPattern patternType);

    void draw() const;
    void draw(size_t i) const;

    bool isReady();

    std::vector<std::vector<cv::Point2f>> imagePoints;

protected:
    CalibrationPattern patternType;
    cv::Size patternSize, addedImageSize, subpixelSize;
    float squareSize;
    cv::Mat grayMat;

    cv::Mat distCoeffs;

    std::vector<cv::Mat> boardRotations, boardTranslations;
    std::vector<std::vector<cv::Point3f>> objectPoints;

    float reprojectionError;
    std::vector<float> perViewErrors;

    bool fillFrame;
    cv::Mat undistortBuffer;
    cv::Mat undistortMapX, undistortMapY;

    void updateObjectPoints();
    void updateReprojectionError();
    void updateUndistortion();

    Intrinsics distortedIntrinsics;
    Intrinsics undistortedIntrinsics;

    bool ready;
};

} // namespace tcx
