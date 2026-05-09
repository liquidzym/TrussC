#pragma once
#include <TrussC.h>
#include <tcxPDSP.h>
using namespace std;
using namespace tc;

class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
private:
    tcx::pdsp::AudioStream stream_;
    tcx::pdsp::SineOsc osc_;
    tcx::pdsp::Gain gain_;
    float freq_ = 440.0f;
};
