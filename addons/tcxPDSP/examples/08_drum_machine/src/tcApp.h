#pragma once
#include <TrussC.h>
#include <tcxPDSP.h>
using namespace std; using namespace tc;
class tcApp:public App{
public:void setup()override;void update()override;void draw()override;void keyPressed(int)override;
private:tcx::pdsp::AudioStream stream_;tcx::pdsp::DrumVoice kick_,snare_,hat_;
tcx::pdsp::Transport transport_;tcx::pdsp::EuclideanSequencer kickSeq_,snareSeq_,hatSeq_;
tcx::pdsp::EventQueue<64> kickQ_,snareQ_,hatQ_;
};
