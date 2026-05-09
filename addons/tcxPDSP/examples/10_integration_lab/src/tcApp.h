#pragma once
#include <TrussC.h>
#include <tcxPDSP.h>
#include <array>

using namespace std;
using namespace tc;

class tcApp : public App {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    void applyEvent(const tcx::pdsp::SequenceEvent& event);

    tcx::pdsp::AudioStream stream_;
    tcx::pdsp::Processor processor_;
    tcx::pdsp::SineOsc osc_;
    tcx::pdsp::Gain gain_;
    tcx::pdsp::Delay delay_;
    tcx::pdsp::Transport transport_;
    tcx::pdsp::StepSequencer sequencer_;
    tcx::pdsp::EventQueue<128> events_;
    tcx::pdsp::RMS rms_;
    tcx::pdsp::PeakMeter peak_;
    tcx::pdsp::EnvelopeFollower env_;
    tcx::pdsp::PitchDetector pitch_;
    tcx::pdsp::OnsetDetector onset_;
    tcx::pdsp::OnePole smoothLevel_;

    float gainTarget_ = 0.0f;
    float rmsVal_ = 0.0f;
    float peakVal_ = 0.0f;
    float envVal_ = 0.0f;
    float pitchVal_ = 0.0f;
    bool onsetVal_ = false;
    bool playing_ = true;
};
