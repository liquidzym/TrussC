#pragma once

// ======================================================================
// tcxCvKalman.h - Kalman filter for position and orientation smoothing
// ======================================================================
//
// Based on OpenCV's KalmanFilter.
//
// Usage (position):
//   KalmanPosition kp;
//   kp.init(0.1f, 0.1f);  // smoothness, rapidness
//   kp.update(tc::Vec3(x, y, z));
//   tc::Vec3 smoothed = kp.getEstimation();
//   tc::Vec3 velocity = kp.getVelocity();
//
// Usage (orientation):
//   KalmanEuler ke;
//   ke.init(0.1f, 0.1f);
//   ke.update(tc::Quaternion(x, y, z, w));
//   tc::Quaternion smoothed = ke.getEstimation();
//

#include <TrussC.h>
#include <opencv2/opencv.hpp>

namespace tcx {

// ======================================================================
// KalmanPosition - Kalman filter for 3D position (and optionally velocity/accel)
// ======================================================================

template <class T>
class KalmanPosition_ {
    cv::KalmanFilter KF;
    cv::Mat_<T> measurement, prediction, estimated;

public:
    // smoothness: smaller = smoother
    // rapidness: smaller = more rapid
    // bUseAccel: include acceleration in state
    void init(T smoothness = (T)0.1, T rapidness = (T)0.1, bool bUseAccel = false);

    void update(const tc::Vec3& p);
    tc::Vec3 getPrediction();
    tc::Vec3 getEstimation();
    tc::Vec3 getVelocity();
};

typedef KalmanPosition_<float> KalmanPosition;

// ======================================================================
// KalmanEuler - Kalman filter for Euler angle orientation
// ======================================================================

template <class T>
class KalmanEuler_ : public KalmanPosition_<T> {
    tc::Vec3 eulerPrev;

public:
    void init(T smoothness = (T)0.1, T rapidness = (T)0.1, bool bUseAccel = false);
    void update(const tc::Quaternion& q);
    tc::Quaternion getPrediction();
    tc::Quaternion getEstimation();
};

typedef KalmanEuler_<float> KalmanEuler;

// ======================================================================
// Template implementations
// ======================================================================

// --- KalmanPosition ---

template <class T>
void KalmanPosition_<T>::init(T smoothness, T rapidness, bool bUseAccel) {
    if (bUseAccel) {
        // 9 variables: position + velocity + acceleration, 3 measurements (position)
        KF.init(9, 3, 0);

        KF.transitionMatrix = (cv::Mat_<T>(9, 9) <<
            1, 0, 0, 1, 0, 0, (T)0.5, 0, 0,
            0, 1, 0, 0, 1, 0, 0, (T)0.5, 0,
            0, 0, 1, 0, 0, 1, 0, 0, (T)0.5,
            0, 0, 0, 1, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 1, 0, 0, 1, 0,
            0, 0, 0, 0, 0, 1, 0, 0, 1,
            0, 0, 0, 0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 0, 0, 0, 1, 0,
            0, 0, 0, 0, 0, 0, 0, 0, 1);

        measurement = cv::Mat_<T>::zeros(3, 1);
        KF.statePre = cv::Mat_<T>::zeros(9, 1);
    } else {
        // 6 variables: position + velocity, 3 measurements (position)
        KF.init(6, 3, 0);

        KF.transitionMatrix = (cv::Mat_<T>(6, 6) <<
            1, 0, 0, 1, 0, 0,
            0, 1, 0, 0, 1, 0,
            0, 0, 1, 0, 0, 1,
            0, 0, 0, 1, 0, 0,
            0, 0, 0, 0, 1, 0,
            0, 0, 0, 0, 0, 1);

        measurement = cv::Mat_<T>::zeros(3, 1);
        KF.statePre = cv::Mat_<T>::zeros(6, 1);
    }

    cv::setIdentity(KF.measurementMatrix);
    cv::setIdentity(KF.processNoiseCov, cv::Scalar::all(smoothness));
    cv::setIdentity(KF.measurementNoiseCov, cv::Scalar::all(rapidness));
    cv::setIdentity(KF.errorCovPost, cv::Scalar::all(0.1));
}

template <class T>
void KalmanPosition_<T>::update(const tc::Vec3& p) {
    prediction = KF.predict();

    measurement(0) = (T)p.x;
    measurement(1) = (T)p.y;
    measurement(2) = (T)p.z;
    estimated = KF.correct(measurement);
}

template <class T>
tc::Vec3 KalmanPosition_<T>::getPrediction() {
    return tc::Vec3((float)prediction(0), (float)prediction(1), (float)prediction(2));
}

template <class T>
tc::Vec3 KalmanPosition_<T>::getEstimation() {
    return tc::Vec3((float)estimated(0), (float)estimated(1), (float)estimated(2));
}

template <class T>
tc::Vec3 KalmanPosition_<T>::getVelocity() {
    return tc::Vec3((float)estimated(3), (float)estimated(4), (float)estimated(5));
}

// --- KalmanEuler ---

template <class T>
void KalmanEuler_<T>::init(T smoothness, T rapidness, bool bUseAccel) {
    KalmanPosition_<T>::init(smoothness, rapidness, bUseAccel);
    eulerPrev = tc::Vec3(0, 0, 0);
}

template <class T>
void KalmanEuler_<T>::update(const tc::Quaternion& q) {
    tc::Vec3 euler = q.toEuler(); // TrussC uses toEuler()

    for (int i = 0; i < 3; i++) {
        float* vals = &euler.x;
        float* prevVals = &eulerPrev.x;
        float rev = floorf((prevVals[i] + 180.0f) / 360.0f) * 360.0f;
        vals[i] += rev;
        if (vals[i] < -90.0f + rev && prevVals[i] > 90.0f + rev) vals[i] += 360.0f;
        else if (vals[i] > 90.0f + rev && prevVals[i] < -90.0f + rev) vals[i] -= 360.0f;
    }

    KalmanPosition_<T>::update(euler);
    eulerPrev = euler;
}

template <class T>
tc::Quaternion KalmanEuler_<T>::getPrediction() {
    tc::Vec3 euler = KalmanPosition_<T>::getPrediction();
    return tc::Quaternion::fromEuler(euler); // TrussC uses fromEuler
}

template <class T>
tc::Quaternion KalmanEuler_<T>::getEstimation() {
    tc::Vec3 euler = KalmanPosition_<T>::getEstimation();
    return tc::Quaternion::fromEuler(euler);
}

} // namespace tcx
