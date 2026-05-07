#pragma once

#include <TrussC.h>
#include <tcxCV.h>

using namespace std;
using namespace tc;
using namespace tcx;

class tcApp : public App {
public:
    Image original, warped;
    vector<cv::Point2f> dstPoints;
    int selectedCorner = -1;

    void setup() override;
    void update() override;
    void draw() override;
    void mousePressed(Vec2 pos, int button) override;
    void mouseDragged(Vec2 pos, int button) override;
    void mouseReleased(Vec2 pos, int button) override;
    void keyPressed(int key) override;
    void createTestImage();
};
