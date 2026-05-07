#pragma once

// ======================================================================
// tcxCV.h - Computer Vision addon for TrussC (port of ofxCv)
// ======================================================================
//
// Provides image processing wrappers, contour finding, optical flow,
// camera calibration, object tracking, background subtraction,
// object detection, Kalman filtering, and more.
//
// Dependencies: tcxOpenCV (for OpenCV + conversion functions)
//
// Usage:
//   #include <tcxCV.h>
//   using namespace std;
//   using namespace tc;
//   using namespace tcx;
//
//   Image img;
//   img.load("photo.jpg");
//   cv::Mat mat = toCv(img);
//   cv::GaussianBlur(mat, mat, cv::Size(5, 5), 0);
//   toOf(mat, img);
//

// OpenCV (provided by tcxOpenCV)
#include <opencv2/opencv.hpp>

// tcxOpenCV conversion utilities
#include <tcxOpenCV.h>

// tcxCV modules
//
// Three layers of utility functions:
#include "tcxCvUtilities.h"  // low-level: toCv, toOf, imitate, allocate
#include "tcxCvWrappers.h"   // mid-level: wrappers that accept toCv-compatible objects
#include "tcxCvHelpers.h"    // high-level: helpers for complex tasks

// All functions guarantee the size of the output with imitate when possible.
// Data is returned via arguments when an expensive copy is required or
// when preallocated buffers should be used. Return values are used when
// data is small or preallocation is unlikely.

// Higher-level classes:
#include "tcxCvDistance.h"          // edit distance
#include "tcxCvCalibration.h"       // camera calibration
#include "tcxCvTracker.h"           // object tracking
#include "tcxCvContourFinder.h"     // contour finding and tracking
#include "tcxCvRunningBackground.h" // background subtraction
#include "tcxCvFlow.h"              // optical flow (PyrLK, Farneback)
#include "tcxCvObjectFinder.h"      // object detection (e.g., face detection)
#include "tcxCvKalman.h"            // Kalman filter for smoothing
