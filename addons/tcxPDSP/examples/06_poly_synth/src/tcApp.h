#pragma once
#include <TrussC.h>
#include <tcxPDSP.h>
#include <map>
using namespace std; using namespace tc;
class tcApp:public App{
public:void setup()override;void update()override;void draw()override;void keyPressed(int)override;void keyReleased(int)override;
private:tcx::pdsp::AudioStream stream_;tcx::pdsp::PolySynth synth_{8};
map<int,int> keyMap_;vector<int> activeNotes_;
void playChord(const vector<int>& notes);
};
