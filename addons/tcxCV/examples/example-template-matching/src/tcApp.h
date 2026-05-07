#pragma once
#include <TrussC.h>
#include <tcxCV.h>
using namespace std;
using namespace tc;
using namespace tcx;

class tcApp : public App {
public:
    Image original, templ, result;
    cv::Point matchLoc;
    double matchVal = 0;

    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void createTestImage();
};
