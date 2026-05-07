#pragma once

#include <TrussC.h>
#include <tcxCV.h>

using namespace std;
using namespace tc;
using namespace tcx;

class tcApp : public App {
public:
    vector<Vec3> sourcePoints, targetPoints;
    Mat4 recoveredTransform;
    Mat4 groundTruthTransform;
    float noiseAmount = 0.05f;
    bool showRecovered = true;

    void setup() override;
    void draw() override;
    void keyPressed(int key) override;
    void regeneratePoints();
};
