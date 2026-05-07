#pragma once
#include <TrussC.h>
#include <tcxCV.h>
using namespace std;
using namespace tc;
using namespace tcx;

class tcApp : public App {
public:
    Image original, processed;
    int iterations = 1;
    int mode = 0; // 0=erode, 1=dilate

    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void createTestImage();
};
