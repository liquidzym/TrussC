#pragma once
#include <TrussC.h>
#include <tcxCV.h>
using namespace std;
using namespace tc;
using namespace tcx;

class tcApp : public App {
public:
    Image original, processed;
    int blurSize = 15;
    int mode = 0; // 0=Gaussian, 1=Box, 2=Median

    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void createTestImage();
};
