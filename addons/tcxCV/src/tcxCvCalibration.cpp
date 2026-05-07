#include "tcxCvCalibration.h"
#include "tcxCvHelpers.h"
#include "tcxCvWrappers.h"
#include <TrussC.h>

namespace tcx {

using namespace cv;
using namespace std;

// ======================================================================
// Intrinsics
// ======================================================================

void Intrinsics::setup(float focalLength_, cv::Size imageSize_, cv::Size2f sensorSize_,
                       cv::Point2d principalPoint_) {
    float focalPixels = (focalLength_ / sensorSize_.width) * imageSize_.width;
    float fx = focalPixels;
    float fy = focalPixels;
    float cx = (float)(imageSize_.width * principalPoint_.x);
    float cy = (float)(imageSize_.height * principalPoint_.y);
    cv::Mat cm = (cv::Mat_<double>(3, 3) <<
                  fx, 0, cx,
                  0, fy, cy,
                  0, 0, 1);
    setup(cm, imageSize_, sensorSize_);
}

void Intrinsics::setup(cv::Mat cameraMatrix_, cv::Size imageSize_, cv::Size2f sensorSize_) {
    cameraMatrix = cameraMatrix_;
    imageSize = imageSize_;
    sensorSize = sensorSize_;
    updateValues();
}

void Intrinsics::updateValues() {
    calibrationMatrixValues(cameraMatrix, imageSize,
                            sensorSize.width, sensorSize.height,
                            fov.x, fov.y,
                            focalLength,
                            principalPoint,
                            aspectRatio);
}

void Intrinsics::setImageSize(cv::Size imgSize) { imageSize = imgSize; }
cv::Mat Intrinsics::getCameraMatrix() const { return cameraMatrix; }
cv::Size Intrinsics::getImageSize() const { return imageSize; }
cv::Size2f Intrinsics::getSensorSize() const { return sensorSize; }
cv::Point2d Intrinsics::getFov() const { return fov; }
double Intrinsics::getFocalLength() const { return focalLength; }
double Intrinsics::getAspectRatio() const { return aspectRatio; }
cv::Point2d Intrinsics::getPrincipalPoint() const { return principalPoint; }

// ======================================================================
// Calibration
// ======================================================================

Calibration::Calibration()
    : patternType(CHESSBOARD)
    , patternSize(cv::Size(10, 7))
    , subpixelSize(cv::Size(11, 11))
    , squareSize(2.5f)
    , reprojectionError(0)
    , distCoeffs(cv::Mat::zeros(8, 1, CV_64F))
    , fillFrame(true)
    , ready(false) {
}

void Calibration::save(const string& filename) const {
    if (!ready) {
        tc::logError("Calibration::save() - calibration not ready");
        return;
    }
    FileStorage fs(filename, FileStorage::WRITE);
    cv::Size imgSize = distortedIntrinsics.getImageSize();
    cv::Size2f sensorSz = distortedIntrinsics.getSensorSize();
    cv::Mat cm = distortedIntrinsics.getCameraMatrix();
    fs << "cameraMatrix" << cm;
    fs << "imageSize_width" << imgSize.width;
    fs << "imageSize_height" << imgSize.height;
    fs << "sensorSize_width" << sensorSz.width;
    fs << "sensorSize_height" << sensorSz.height;
    fs << "distCoeffs" << distCoeffs;
    fs << "reprojectionError" << reprojectionError;
    fs << "features" << "[";
    for (int i = 0; i < (int)imagePoints.size(); i++) {
        fs << imagePoints[i];
    }
    fs << "]";
}

void Calibration::load(const string& filename) {
    imagePoints.clear();
    FileStorage fs(filename, FileStorage::READ);
    cv::Size imgSize;
    cv::Size2f sensorSz;
    cv::Mat cm;
    fs["cameraMatrix"] >> cm;
    fs["imageSize_width"] >> imgSize.width;
    fs["imageSize_height"] >> imgSize.height;
    fs["sensorSize_width"] >> sensorSz.width;
    fs["sensorSize_height"] >> sensorSz.height;
    fs["distCoeffs"] >> distCoeffs;
    fs["reprojectionError"] >> reprojectionError;
    FileNode features = fs["features"];
    for (FileNodeIterator it = features.begin(); it != features.end(); it++) {
        vector<Point2f> cur;
        (*it) >> cur;
        imagePoints.push_back(cur);
    }
    addedImageSize = imgSize;
    distortedIntrinsics.setup(cm, imgSize, sensorSz);
    updateUndistortion();
    ready = true;
}

void Calibration::reset() {
    ready = false;
    reprojectionError = 0.0f;
    imagePoints.clear();
    objectPoints.clear();
    perViewErrors.clear();
}

void Calibration::setPatternType(CalibrationPattern type) { patternType = type; }
void Calibration::setPatternSize(int xCount, int yCount) { patternSize = cv::Size(xCount, yCount); }
void Calibration::setSquareSize(float sz) { squareSize = sz; }
void Calibration::setFillFrame(bool ff) { fillFrame = ff; }

void Calibration::setSubpixelSize(int sz) {
    sz = std::max(sz, 2);
    subpixelSize = cv::Size(sz, sz);
}

bool Calibration::add(Mat img) {
    addedImageSize = img.size();
    vector<Point2f> pointBuf;
    bool found = findBoard(img, pointBuf);
    if (found) {
        imagePoints.push_back(pointBuf);
    } else {
        tc::logError("Calibration::add() - pattern not found, check patternSize or lighting");
    }
    return found;
}

bool Calibration::findBoard(Mat img, vector<Point2f>& pointBuf, bool refine) {
    bool found = false;
    if (patternType == CHESSBOARD) {
        int chessFlags = CALIB_CB_ADAPTIVE_THRESH;
        found = findChessboardCorners(img, patternSize, pointBuf, chessFlags);
        if (found && refine) {
            if (img.type() != CV_8UC1) {
                copyGray(img, grayMat);
            } else {
                grayMat = img;
            }
            cornerSubPix(grayMat, pointBuf, subpixelSize,
                         cv::Size(-1, -1),
                         cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::COUNT, 30, 0.1));
        }
    }
    // Note: circles grid not implemented (removed from OpenCV 4.x contrib)
    return found;
}

bool Calibration::clean(float minReprojectionError_) {
    int removed = 0;
    for (int i = (int)size() - 1; i >= 0; i--) {
        if (getReprojectionError(i) > minReprojectionError_) {
            objectPoints.erase(objectPoints.begin() + i);
            imagePoints.erase(imagePoints.begin() + i);
            removed++;
        }
    }
    if (size() > 0) {
        if (removed > 0) {
            return calibrate();
        }
        return true;
    }
    tc::logError("Calibration::clean() - removed last object/image point pair");
    return false;
}

bool Calibration::calibrate() {
    if (size() < 1) {
        tc::logError("Calibration::calibrate() - no image data");
        if (ready) {
            tc::logNotice("Calibration::calibrate() - already calibrated via load()");
        }
        return ready;
    }

    Mat cameraMatrix_ = Mat::eye(3, 3, CV_64F);
    updateObjectPoints();

    int calibFlags = 0;
    double rms = calibrateCamera(objectPoints, imagePoints, addedImageSize,
                                  cameraMatrix_, distCoeffs,
                                  boardRotations, boardTranslations, calibFlags);
    tc::logNotice() << "Calibration RMS error: " << rms;

    ready = checkRange(cameraMatrix_) && checkRange(distCoeffs);

    if (!ready) {
        tc::logError("Calibration::calibrate() - failed");
    }

    distortedIntrinsics.setup(cameraMatrix_, addedImageSize);
    updateReprojectionError();
    updateUndistortion();

    return ready;
}

bool Calibration::isReady() {
    return ready;
}

bool Calibration::calibrateFromDirectory(const string& directory) {
    // This is a convenience method that requires filesystem access.
    // Users can also call add() directly for each image.
    tc::logError("Calibration::calibrateFromDirectory() not yet implemented in tcxCV. Use add() for each image instead.");
    return false;
}

void Calibration::setIntrinsics(Intrinsics& distortedIntrinsics_) {
    distortedIntrinsics = distortedIntrinsics_;
    addedImageSize = distortedIntrinsics.getImageSize();
    updateUndistortion();
    ready = true;
}

void Calibration::setDistortionCoefficients(float k1, float k2, float p1, float p2,
                                            float k3, float k4, float k5, float k6) {
    distCoeffs.at<double>(0) = k1;
    distCoeffs.at<double>(1) = k2;
    distCoeffs.at<double>(2) = p1;
    distCoeffs.at<double>(3) = p2;
    distCoeffs.at<double>(4) = k3;
    distCoeffs.at<double>(5) = k4;
    distCoeffs.at<double>(6) = k5;
    distCoeffs.at<double>(7) = k6;
}

void Calibration::undistort(Mat img, int interpolationMode) {
    if (img.rows != undistortMapX.rows || img.cols != undistortMapX.cols) {
        tc::logError("Calibration::undistort() - image size mismatch with undistort map");
        return;
    }
    img.copyTo(undistortBuffer);
    undistort(undistortBuffer, img, interpolationMode);
}

void Calibration::undistort(Mat src, Mat dst, int interpolationMode) {
    remap(src, dst, undistortMapX, undistortMapY, interpolationMode);
}

tc::Vec2 Calibration::undistort(tc::Vec2& src) const {
    tc::Vec2 dst;
    Mat matSrc = Mat(1, 1, CV_32FC2, &src.x);
    Mat matDst = Mat(1, 1, CV_32FC2, &dst.x);
    undistortPoints(matSrc, matDst, distortedIntrinsics.getCameraMatrix(), distCoeffs);
    return dst;
}

void Calibration::undistort(vector<tc::Vec2>& src, vector<tc::Vec2>& dst) const {
    int n = (int)src.size();
    dst.resize(n);
    Mat matSrc = Mat(n, 1, CV_32FC2, &src[0].x);
    Mat matDst = Mat(n, 1, CV_32FC2, &dst[0].x);
    undistortPoints(matSrc, matDst, distortedIntrinsics.getCameraMatrix(), distCoeffs);
}

bool Calibration::getTransformation(Calibration& dst, Mat& rotation, Mat& translation) {
    if (!ready) {
        tc::logError("Calibration::getTransformation() - not calibrated");
        return false;
    }
    if (imagePoints.size() != dst.imagePoints.size() || patternSize != dst.patternSize) {
        tc::logError("Calibration::getTransformation() - both must be trained on same board");
        return false;
    }

    Mat fundamentalMatrix, essentialMatrix;
    Mat cameraMatrix_ = distortedIntrinsics.getCameraMatrix();
    Mat dstCameraMatrix = dst.getDistortedIntrinsics().getCameraMatrix();

    stereoCalibrate(objectPoints,
                    imagePoints, dst.imagePoints,
                    cameraMatrix_, distCoeffs,
                    dstCameraMatrix, dst.distCoeffs,
                    distortedIntrinsics.getImageSize(),
                    rotation, translation,
                    essentialMatrix, fundamentalMatrix);
    return true;
}

float Calibration::getReprojectionError() const { return reprojectionError; }
float Calibration::getReprojectionError(int i) const { return perViewErrors[i]; }
const Intrinsics& Calibration::getDistortedIntrinsics() const { return distortedIntrinsics; }
const Intrinsics& Calibration::getUndistortedIntrinsics() const { return undistortedIntrinsics; }
cv::Mat Calibration::getDistCoeffs() const { return distCoeffs; }
size_t Calibration::size() const { return imagePoints.size(); }
cv::Size Calibration::getPatternSize() const { return patternSize; }
float Calibration::getSquareSize() const { return squareSize; }

void Calibration::draw() const {
    for (size_t i = 0; i < imagePoints.size(); i++) {
        draw(i);
    }
}

void Calibration::draw(size_t i) const {
    for (size_t j = 0; j < imagePoints[i].size(); j++) {
        tc::Vec2 pt = toOf(imagePoints[i][j]);
        tc::drawCircle(pt.x, pt.y, 5);
    }
}

void Calibration::updateObjectPoints() {
    vector<Point3f> points = createObjectPoints(patternSize, squareSize, patternType);
    objectPoints.resize(imagePoints.size(), points);
}

void Calibration::updateReprojectionError() {
    vector<Point2f> imagePoints2;
    int totalPoints = 0;
    double totalErr = 0;

    perViewErrors.clear();
    perViewErrors.resize(objectPoints.size());

    for (size_t i = 0; i < objectPoints.size(); i++) {
        projectPoints(Mat(objectPoints[i]), boardRotations[i], boardTranslations[i],
                      distortedIntrinsics.getCameraMatrix(), distCoeffs, imagePoints2);
        double err = norm(Mat(imagePoints[i]), Mat(imagePoints2), NORM_L2);
        int n = (int)objectPoints[i].size();
        perViewErrors[i] = (float)sqrt(err * err / n);
        totalErr += err * err;
        totalPoints += n;
    }

    reprojectionError = (float)sqrt(totalErr / totalPoints);
}

void Calibration::updateUndistortion() {
    Mat undistortedCameraMatrix = getOptimalNewCameraMatrix(
        distortedIntrinsics.getCameraMatrix(), distCoeffs,
        distortedIntrinsics.getImageSize(), fillFrame ? 0 : 1);
    initUndistortRectifyMap(distortedIntrinsics.getCameraMatrix(), distCoeffs,
                            Mat(), undistortedCameraMatrix,
                            distortedIntrinsics.getImageSize(),
                            CV_16SC2, undistortMapX, undistortMapY);
    undistortedIntrinsics.setup(undistortedCameraMatrix, distortedIntrinsics.getImageSize());
}

vector<Point3f> Calibration::createObjectPoints(cv::Size patternSize_, float squareSize_,
                                                CalibrationPattern patternType_) {
    vector<Point3f> corners;
    switch (patternType_) {
        case CHESSBOARD:
        case CIRCLES_GRID:
            for (int i = 0; i < patternSize_.height; i++)
                for (int j = 0; j < patternSize_.width; j++)
                    corners.push_back(Point3f((float)(j * squareSize_), (float)(i * squareSize_), 0));
            break;
        case ASYMMETRIC_CIRCLES_GRID:
            for (int i = 0; i < patternSize_.height; i++)
                for (int j = 0; j < patternSize_.width; j++)
                    corners.push_back(Point3f((float)(((2 * j) + (i % 2)) * squareSize_),
                                              (float)(i * squareSize_), 0));
            break;
    }
    return corners;
}

} // namespace tcx
