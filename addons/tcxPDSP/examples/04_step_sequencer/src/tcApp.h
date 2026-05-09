#pragma once
#include <TrussC.h>
#include <tcxPDSP.h>
using namespace std; using namespace tc;
class tcApp:public App{
public:void setup()override;void update()override;void draw()override;void keyPressed(int)override;
private:tcx::pdsp::AudioStream stream_;tcx::pdsp::MonoSynth synth_;
tcx::pdsp::Transport transport_;tcx::pdsp::StepSequencer seq_;tcx::pdsp::EventQueue<1024> queue_;
int step_=0;
};
