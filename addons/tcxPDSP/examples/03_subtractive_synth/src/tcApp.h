#pragma once
#include <TrussC.h>
#include <tcxPDSP.h>
#include <map>
using namespace std; using namespace tc;
class tcApp:public App{
public:void setup()override;void update()override;void draw()override;void keyPressed(int)override;void keyReleased(int)override;
private:tcx::pdsp::AudioStream stream_;tcx::pdsp::MonoSynth synth_;tcx::pdsp::RMS rms_;
map<int,int> keyMap_;float peak_=0,cutoff_=2000;
};
