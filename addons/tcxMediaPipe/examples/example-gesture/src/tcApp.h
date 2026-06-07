#pragma once

#include <TrussC.h>
#include <tcxMediaPipe.h>

using namespace std;
using namespace tc;

class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;

private:
    tcx::mediapipe::MediaPipe mediaPipe_;
    string setupError_;
};
