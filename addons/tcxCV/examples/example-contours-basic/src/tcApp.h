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
    float thresholdVal = 128;
    bool invert = false;
    bool findHoles = false;

    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void createTestImage();
};
