#pragma once
#include <TrussC.h>
#include <tcxPDSP.h>
using namespace std; using namespace tc;
class tcApp:public App{
public:void setup()override;void update()override;void draw()override;void keyPressed(int)override;
private:tcx::pdsp::AudioStream stream_;tcx::pdsp::GranularSynth gran_;tcx::pdsp::PhysicalModel pm_;
int mode_=0;float density_=10,grainSize_=0.1f,pitch_=1,pmFreq_=220;
};
