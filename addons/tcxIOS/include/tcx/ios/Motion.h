#pragma once

#include "Types.h"

namespace tcx::ios {

struct MotionVector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct MotionSample {
    MotionVector3 acceleration;
    MotionVector3 rotationRate;
    MotionVector3 gravity;
    MotionVector3 userAcceleration;
    double attitudeRoll = 0.0;
    double attitudePitch = 0.0;
    double attitudeYaw = 0.0;
    double timestampSeconds = 0.0;
    bool hasDeviceMotion = false;
};

struct MotionConfig {
    double updateIntervalSeconds = 1.0 / 60.0;
    bool useDeviceMotion = true;
};

class Motion {
public:
    void start(const MotionConfig& config, Completion<void> done);
    void stop();
    bool isRunning() const;
    bool latest(MotionSample& out) const;
};

Motion& motion();

} // namespace tcx::ios
