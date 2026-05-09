#pragma once
// PulseOsc — Variable pulse-width square wave oscillator
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include <cmath>
namespace tcx::pdsp {
class PulseOsc : public AudioNode {
public:
    PulseOsc():out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{sr_=(float)ctx.sampleRate;freq_.prepare(ctx.sampleRate,5);pw_.prepare(ctx.sampleRate,5);prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){
            float fr=freq_.next(),pw=pw_.next(),inc=fr/sr_;
            phase_+=inc;if(phase_>=1)phase_-=1;
            o[i]=phase_<pw?1.0f:-1.0f;
        }
    }
    PatchNode& out(){return out_;}
    void setFrequency(float hz){freq_.set(hz);}
    float getFrequency()const{return freq_.getTarget();}
    void setPulseWidth(float w){pw_.set(w);} // 0~1, 0.5=square
private:Parameter freq_{440},pw_{0.5f};PatchNode out_;float sr_=48000,phase_=0;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};
}
