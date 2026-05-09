#pragma once
#include <TrussC.h>
#include <tcxPDSP.h>
using namespace std; using namespace tc;
class tcApp:public App{
public:void setup()override;void update()override;void draw()override;void keyPressed(int)override;
private:tcx::pdsp::AudioStream stream_;tcx::pdsp::Processor processor_;tcx::pdsp::Mixer mixer_;
tcx::pdsp::SineOsc osc1_,osc2_;tcx::pdsp::Noise noise_;tcx::pdsp::Delay delay_;
float f1_=220,f2_=330;bool noiseOn_=false;};
