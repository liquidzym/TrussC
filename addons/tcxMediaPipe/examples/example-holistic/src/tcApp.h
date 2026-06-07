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
    void drawHandLandmarks(const tcx::mediapipe::HandResult& result, float width, float height);
    void drawPoseLandmarks(const tcx::mediapipe::PoseResult& result, float width, float height);
    void drawFaceLandmarks(const tcx::mediapipe::FaceResult& result, float width, float height);

    tcx::mediapipe::MediaPipe mediaPipe_;
    string setupError_;
};
