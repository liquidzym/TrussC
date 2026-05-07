#pragma once

// =============================================================================
// tcxOpenCV.h - OpenCV integration for TrussC
// =============================================================================
//
// This addon provides minimal glue between TrussC and OpenCV.
// Use OpenCV (cv::) directly for image processing - this addon just handles
// type conversion between tc:: and cv:: types.
//
// Usage:
//   #include <tcxOpenCV.h>
//   using namespace tc;
//
//   Image img;
//   img.load("photo.jpg");
//
//   cv::Mat mat = tcx::toCvMat(img);
//   cv::GaussianBlur(mat, mat, cv::Size(5, 5), 0);
//   tcx::toTcImage(mat, img);
//
//   img.draw(0, 0);
//

// OpenCV headers
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/calib3d.hpp>

// TrussC integration
#include "tcxCvConvert.h"
