#pragma once
#include <TrussC.h>
#include <tcxPDSP.h>
using namespace std; using namespace tc;
class tcApp:public App{
public:void setup()override;void update()override;void draw()override;void keyPressed(int)override;
private:tcx::pdsp::AudioStream stream_;tcx::pdsp::SineOsc osc_;tcx::pdsp::Noise noise_;
tcx::pdsp::RMS rms_;tcx::pdsp::PeakMeter peak_;tcx::pdsp::EnvelopeFollower env_;
float rmsVal_=0,peakVal_=0,envVal_=0;bool noiseOn_=false;
};
