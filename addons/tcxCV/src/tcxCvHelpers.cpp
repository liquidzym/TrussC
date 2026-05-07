#include "tcxCvHelpers.h"
#include "tcxCvUtilities.h"
#include "tcxCvWrappers.h"
#include <TrussC.h>

namespace tcx {

using namespace std;
using namespace cv;

// ======================================================================
// Drawing helpers
// ======================================================================

void drawMat(const Mat& mat, float x, float y) {
    drawMat(mat, x, y, (float)mat.cols, (float)mat.rows);
}

void drawMat(const Mat& mat, float x, float y, float width, float height) {
    if (mat.empty()) {
        return;
    }
    // Convert cv::Mat to tc::Image and draw it
    tc::Image img = toOf(mat);
    img.draw(x, y, width, height);
}

// ======================================================================
// Matrix helpers
// ======================================================================

tc::Mat4 makeMatrix(Mat rotation, Mat translation) {
    Mat rot3x3;
    if (rotation.rows == 3 && rotation.cols == 3) {
        rot3x3 = rotation;
    } else {
        Rodrigues(rotation, rot3x3);
    }
    double* rm = rot3x3.ptr<double>(0);
    double* tm = translation.ptr<double>(0);

    tc::Mat4 result;
    result.m[0] = (float)rm[0];
    result.m[1] = (float)rm[3];
    result.m[2] = (float)rm[6];
    result.m[3] = 0;
    result.m[4] = (float)rm[1];
    result.m[5] = (float)rm[4];
    result.m[6] = (float)rm[7];
    result.m[7] = 0;
    result.m[8] = (float)rm[2];
    result.m[9] = (float)rm[5];
    result.m[10] = (float)rm[8];
    result.m[11] = 0;
    result.m[12] = (float)tm[0];
    result.m[13] = (float)tm[1];
    result.m[14] = (float)tm[2];
    result.m[15] = 1;

    return result;
}

// ======================================================================
// Array search helpers
// ======================================================================

int findFirst(const Mat& arr, unsigned char target) {
    for (int i = 0; i < arr.rows; i++) {
        if (arr.at<unsigned char>(i) == target) {
            return i;
        }
    }
    return 0;
}

int findLast(const Mat& arr, unsigned char target) {
    for (int i = arr.rows - 1; i >= 0; i--) {
        if (arr.at<unsigned char>(i) == target) {
            return i;
        }
    }
    return 0;
}

// ======================================================================
// Line / contour analysis
// ======================================================================

float weightedAverageAngle(const vector<Vec4i>& lines) {
    float angleSum = 0;
    float weights = 0;
    for (size_t i = 0; i < lines.size(); i++) {
        tc::Vec2 start((float)lines[i][0], (float)lines[i][1]);
        tc::Vec2 end((float)lines[i][2], (float)lines[i][3]);
        tc::Vec2 diff = end - start;
        float length = diff.length();
        float weight = length * length;
        float angle = atan2f(diff.y, diff.x);
        angleSum += angle * weight;
        weights += weight;
    }
    return angleSum / weights;
}

vector<cv::Point2f> getConvexPolygon(const vector<cv::Point2f>& convexHull, int targetPoints) {
    vector<cv::Point2f> result = convexHull;

    static const unsigned int maxIterations = 16;
    static const double infinity = numeric_limits<double>::infinity();
    double minEpsilon = 0;
    double maxEpsilon = infinity;
    double curEpsilon = 16; // good initial guess

    if ((int)result.size() > targetPoints) {
        for (unsigned int i = 0; i < maxIterations; i++) {
            approxPolyDP(Mat(convexHull), result, curEpsilon, true);
            if ((int)result.size() == targetPoints) {
                break;
            }
            if ((int)result.size() > targetPoints) {
                minEpsilon = curEpsilon;
                if (maxEpsilon == infinity) {
                    curEpsilon = curEpsilon * 2;
                } else {
                    curEpsilon = (maxEpsilon + minEpsilon) / 2;
                }
            }
            if ((int)result.size() < targetPoints) {
                maxEpsilon = curEpsilon;
                curEpsilon = (maxEpsilon + minEpsilon) / 2;
            }
        }
    }

    return result;
}

// ======================================================================
// Thinning (Zhang-Suen algorithm)
// ======================================================================

void thinningIteration(Mat& img, int iter, Mat& marker) {
    CV_Assert(img.channels() == 1);
    CV_Assert(img.depth() != sizeof(uchar));
    CV_Assert(img.rows > 3 && img.cols > 3);

    int nRows = img.rows;
    int nCols = img.cols;

    if (img.isContinuous()) {
        nCols *= nRows;
        nRows = 1;
    }

    uchar* pAbove;
    uchar* pCurr;
    uchar* pBelow;
    uchar *nw, *no_ptr, *ne;  // north
    uchar *we, *me, *ea;
    uchar *sw, *so, *se;       // south

    uchar* pDst;

    pAbove = NULL;
    pCurr  = img.ptr<uchar>(0);
    pBelow = img.ptr<uchar>(1);

    for (int y = 1; y < img.rows - 1; ++y) {
        pAbove = pCurr;
        pCurr  = pBelow;
        pBelow = img.ptr<uchar>(y + 1);

        pDst = marker.ptr<uchar>(y);

        no_ptr = &(pAbove[0]);
        ne = &(pAbove[1]);
        me = &(pCurr[0]);
        ea = &(pCurr[1]);
        so = &(pBelow[0]);
        se = &(pBelow[1]);

        for (int x = 1; x < img.cols - 1; ++x) {
            nw = no_ptr;
            no_ptr = ne;
            ne = &(pAbove[x + 1]);
            we = me;
            me = ea;
            ea = &(pCurr[x + 1]);
            sw = so;
            so = se;
            se = &(pBelow[x + 1]);

            if (*me == 0) continue; // skip zeroed pixels

            int A = (*no_ptr == 0 && *ne == 1) + (*ne == 0 && *ea == 1) +
                    (*ea == 0 && *se == 1) + (*se == 0 && *so == 1) +
                    (*so == 0 && *sw == 1) + (*sw == 0 && *we == 1) +
                    (*we == 0 && *nw == 1) + (*nw == 0 && *no_ptr == 1);
            if (A != 1) continue;

            int B = *no_ptr + *ne + *ea + *se + *so + *sw + *we + *nw;
            if (B < 2 || B > 6) continue;

            int m1 = iter == 0 ? (*no_ptr * *ea * *so) : (*no_ptr * *ea * *we);
            if (m1) continue;

            int m2 = iter == 0 ? (*ea * *so * *we) : (*no_ptr * *so * *we);
            if (m2) continue;

            pDst[x] = 1;
        }
    }

    img &= ~marker;
}

} // namespace tcx
