#pragma once

#include <TrussC.h>
#include <tcxCV.h>

using namespace std;
using namespace tc;
using namespace tcx;

class tcApp : public App {
public:
    Image original;
    ContourFinder finder;
    Color targetColor = Color(1, 0, 0, 1);
    float thresholdVal = 32;

    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void mousePressed(Vec2 pos, int button) override;
    void createTestImage();
};
