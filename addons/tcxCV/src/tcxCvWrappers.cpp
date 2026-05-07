#include "tcxCvWrappers.h"
#include "tcxCvUtilities.h"
#include <TrussC.h>

namespace tcx {

using namespace std;
using namespace cv;

// ======================================================================
// File I/O
// ======================================================================

void loadMat(Mat& mat, const string& filename) {
    FileStorage fs(filename, FileStorage::READ);
    fs["Mat"] >> mat;
}

void saveMat(Mat mat, const string& filename) {
    FileStorage fs(filename, FileStorage::WRITE);
    fs << "Mat" << mat;
}

void saveImage(Mat& mat, const string& filename) {
    if (mat.depth() == CV_8U) {
        tc::Image img = toOf(mat);
        img.save(filename);
    } else {
        // For non-8U, normalize to 8U first
        Mat normalized;
        if (mat.depth() == CV_16U) {
            mat.convertTo(normalized, CV_8U, 1.0 / 256.0);
        } else if (mat.depth() == CV_32F) {
            mat.convertTo(normalized, CV_8U, 255.0);
        } else if (mat.depth() == CV_64F) {
            mat.convertTo(normalized, CV_8U, 255.0);
        } else {
            normalized = mat;
        }
        tc::Image img = toOf(normalized);
        img.save(filename);
    }
}

int forceOdd(int x) {
    return (x / 2) * 2 + 1;
}

// ======================================================================
// convertColor (single color)
// ======================================================================

Vec3b convertColor(Vec3b color, int code) {
    Mat_<Vec3b> mat(1, 1, CV_8UC3);
    mat(0, 0) = color;
    cvtColor(mat, mat, code);
    return mat(0, 0);
}

tc::Color convertColor(tc::Color color, int code) {
    Vec3b cvColor(
        static_cast<uchar>(color.r * 255),
        static_cast<uchar>(color.g * 255),
        static_cast<uchar>(color.b * 255)
    );
    Vec3b result = convertColor(cvColor, code);
    return tc::Color(
        result[0] / 255.0f,
        result[1] / 255.0f,
        result[2] / 255.0f,
        color.a
    );
}

// ======================================================================
// Polyline / contour geometry
// ======================================================================

tc::Path convexHull(const tc::Path& path) {
    vector<cv::Point2f> contour = toCv(path);
    vector<cv::Point2f> hull;
    cv::convexHull(Mat(contour), hull);
    return toOf(hull);
}

vector<Vec4i> convexityDefects(const vector<cv::Point>& contour) {
    vector<Vec4i> defectsVec;

    if (contour.size() < 4) {
        return defectsVec;
    }

    vector<int> hullIndices;
    cv::convexHull(contour, hullIndices, false, false);

    if (hullIndices.size() < 3) {
        return defectsVec;
    }

    // Sort indices for OpenCV 4.x compatibility
    sort(hullIndices.begin(), hullIndices.end());

    vector<Vec4i> defects;
    try {
        cv::convexityDefects(contour, hullIndices, defects);
    } catch (const cv::Exception&) {
        return defectsVec;
    }

    for (size_t i = 0; i < defects.size(); i++) {
        int startIdx = defects[i][0];
        int endIdx = defects[i][1];
        int farIdx = defects[i][2];

        if (startIdx < 0 || startIdx >= (int)contour.size() ||
            endIdx < 0 || endIdx >= (int)contour.size() ||
            farIdx < 0 || farIdx >= (int)contour.size()) {
            continue;
        }

        Vec4i defect;
        defect[0] = contour[farIdx].x;
        defect[1] = contour[farIdx].y;
        defect[2] = (contour[startIdx].x + contour[endIdx].x) / 2;
        defect[3] = (contour[startIdx].y + contour[endIdx].y) / 2;
        defectsVec.push_back(defect);
    }

    return defectsVec;
}

vector<Vec4i> convexityDefects(const tc::Path& path) {
    vector<cv::Point2f> contour2f = toCv(path);
    vector<cv::Point> contour2i;
    Mat(contour2f).copyTo(contour2i);
    return convexityDefects(contour2i);
}

RotatedRect minAreaRect(const tc::Path& path) {
    return cv::minAreaRect(Mat(toCv(path)));
}

RotatedRect fitEllipse(const tc::Path& path) {
    return cv::fitEllipse(Mat(toCv(path)));
}

void fitLine(const tc::Path& path, tc::Vec2& point, tc::Vec2& direction) {
    Vec4f line;
    cv::fitLine(Mat(toCv(path)), line, cv::DIST_L2, 0, 0.01, 0.01);
    direction = tc::Vec2(line[0], line[1]);
    point = tc::Vec2(line[2], line[3]);
}

// ======================================================================
// 3D affine estimation
// ======================================================================

tc::Mat4 estimateAffine3D(vector<tc::Vec3>& from, vector<tc::Vec3>& to, float accuracy) {
    if (from.size() != to.size() || from.size() == 0 || to.size() == 0) {
        return tc::Mat4();
    }
    vector<unsigned char> outliers;
    return estimateAffine3D(from, to, outliers, accuracy);
}

tc::Mat4 estimateAffine3D(vector<tc::Vec3>& from, vector<tc::Vec3>& to,
                          vector<unsigned char>& outliers, float accuracy) {
    Mat fromMat(1, (int)from.size(), CV_32FC3, &from[0]);
    Mat toMat(1, (int)to.size(), CV_32FC3, &to[0]);
    Mat affine;
    cv::estimateAffine3D(fromMat, toMat, affine, outliers, 3.0, accuracy);

    // Convert 3x4 affine to 4x4 matrix
    tc::Mat4 result;
    if (!affine.empty()) {
        result.m[0] = (float)affine.at<double>(0, 0);
        result.m[1] = (float)affine.at<double>(0, 1);
        result.m[2] = (float)affine.at<double>(0, 2);
        result.m[3] = 0;
        result.m[4] = (float)affine.at<double>(1, 0);
        result.m[5] = (float)affine.at<double>(1, 1);
        result.m[6] = (float)affine.at<double>(1, 2);
        result.m[7] = 0;
        result.m[8] = (float)affine.at<double>(2, 0);
        result.m[9] = (float)affine.at<double>(2, 1);
        result.m[10] = (float)affine.at<double>(2, 2);
        result.m[11] = 0;
        result.m[12] = (float)affine.at<double>(0, 3);
        result.m[13] = (float)affine.at<double>(1, 3);
        result.m[14] = (float)affine.at<double>(2, 3);
        result.m[15] = 1;
    }

    return result;
}

} // namespace tcx
