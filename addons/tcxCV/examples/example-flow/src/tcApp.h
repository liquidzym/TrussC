#pragma once
#include <TrussC.h>
#include <tcxCV.h>
using namespace std;
using namespace tc;
using namespace tcx;

class tcApp : public App {
public:
    Image prevImg, curImg, display;
    FlowFarneback flow;
    float offsetX = 0, offsetY = 0;

    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void generatePattern(Image& img, float ox, float oy);
};
