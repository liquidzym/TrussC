#pragma once
// Chorus/Flanger — modulated delay line
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include <vector>
#include <cmath>
namespace tcx::pdsp {
class Chorus : public AudioNode {
public:
    Chorus():in_(this,0),out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{outputBuffer.allocate(1,ctx.bufferSize);sr_=(float)ctx.sampleRate;rate_.prepare(ctx.sampleRate,10);depth_.prepare(ctx.sampleRate,10);fb_.prepare(ctx.sampleRate,10);wet_.prepare(ctx.sampleRate,10);
        int maxD=(int)(sr_*0.05f);delay_.resize(maxD,0);prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);copyConnectedInput(in_,f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){
            float r=rate_.next(),d=depth_.next(),fb=fb_.next(),w=wet_.next();
            int maxD=(int)delay_.size();
            float mod=sinf(phase_*6.2831853f)*d*sr_*0.002f+sr_*0.005f;
            phase_+=r/sr_;if(phase_>=1)phase_-=1;
            float readPos=(float)writePos_-mod;if(readPos<0)readPos+=maxD;
            int r0=(int)readPos%maxD,r1=(r0+1)%maxD;float frac=readPos-floorf(readPos);
            float delayed=delay_[r0]*(1-frac)+delay_[r1]*frac;
            delay_[writePos_]=o[i]+delayed*fb;writePos_=(writePos_+1)%maxD;
            o[i]=o[i]*(1-w)+delayed*w;
        }
    }
    PatchNode& in(){return in_;}PatchNode& out(){return out_;}
    void setRate(float hz){rate_.set(hz);}void setDepth(float d){depth_.set(d);}
    void setFeedback(float f){fb_.set(f);}void setWet(float w){wet_.set(w);}
private:Parameter rate_{0.5f},depth_{0.5f},fb_{0.3f},wet_{0.5f};PatchNode in_,out_;float sr_=48000,phase_=0;
    std::vector<float> delay_;int writePos_=0;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};
}
