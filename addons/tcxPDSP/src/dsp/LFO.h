#pragma once
// Low-frequency oscillator — outputs modulation signal (sine/tri/saw/square)
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include <cmath>
namespace tcx::pdsp {
class LFO : public AudioNode {
public:
    enum Wave { Sine, Triangle, Saw, Square };
    LFO() : out_(this,0) { outputBuffer.allocate(1,256); }
    void prepare(AudioContext& ctx) override { sr_=(float)ctx.sampleRate; freq_.prepare(ctx.sampleRate,5); prepared_=true; }
    void process(AudioContext& ctx, int f) override {
        if(!prepared_||!active_)return; ensureBuf(f); float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++) {
            float fr=freq_.next(), inc=fr/sr_; phase_+=inc; if(phase_>=1)phase_-=1;
            switch(wave_){case Sine:o[i]=sinf(phase_*6.2831853f);break;case Triangle:o[i]=phase_<0.5f?4*phase_-1:3-4*phase_;break;case Saw:o[i]=2*phase_-1;break;case Square:o[i]=phase_<0.5f?1:-1;break;}
        }
    }
    PatchNode& out(){return out_;}
    void setFrequency(float hz){freq_.set(hz);}
    void setWave(Wave w){wave_=w;}
private:
    Parameter freq_{1.0f}; PatchNode out_; Wave wave_=Sine; float sr_=48000,phase_=0;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};
}
