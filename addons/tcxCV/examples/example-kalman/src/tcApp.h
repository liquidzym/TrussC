#pragma once

#include <TrussC.h>
#include <tcxCV.h>

using namespace std;
using namespace tc;
using namespace tcx;

class tcApp : public App {
public:
    KalmanPosition kp;
    Vec2 rawPos = Vec2(320, 240);
    Vec2 filteredPos = Vec2(320, 240);
    vector<Vec2> rawTrail;
    vector<Vec2> filteredTrail;
    int trailMax = 60;

    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
};
