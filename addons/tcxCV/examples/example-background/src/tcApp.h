#pragma once
#include <TrussC.h>
#include <tcxCV.h>
using namespace std;
using namespace tc;
using namespace tcx;

class tcApp : public App {
public:
    Image frame, fgMask;
    RunningBackground bg;
    float ballX = 150, ballY = 150;
    float ballVX = 2, ballVY = 1.5f;

    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void createFrame(Image& img, float bx, float by);
};
