#pragma once
#include <TrussC.h>
#include <tcxPDSP.h>
using namespace std; using namespace tc;
class tcApp:public App{
public:void setup()override;void update()override;void draw()override;void keyPressed(int)override;void keyReleased(int)override;
private:tcx::pdsp::AudioStream stream_;tcx::pdsp::FMSynth synth_;
float ratio_=2,index_=2;int note_=0;
};
