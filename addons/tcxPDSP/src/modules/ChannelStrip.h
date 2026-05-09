#pragma once
// ChannelStrip — Gain → EQ → Pan → output convenience strip
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "dsp/Gain.h"
#include "dsp/Biquad.h"
#include "dsp/Panner.h"
namespace tcx::pdsp {
class ChannelStrip : public AudioNode {
public:
    ChannelStrip():in_(this,0),out_(this,0){outputBuffer.allocate(2,256);}
    void prepare(AudioContext& ctx)override{gain_.prepare(ctx);eq_.prepare(ctx);pan_.prepare(ctx);prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);
        float*L=outputBuffer.channel(0),*R=outputBuffer.channel(1);
        // Simple: copy gain output to both L/R, then pan
        gain_.process(ctx,f);eq_.process(ctx,f);
        float*g=gain_.output().channel(0),*e=eq_.output().channel(0);
        for(int i=0;i<f;i++){L[i]=e[i];R[i]=e[i];}
        pan_.process(ctx,f);
    }
    PatchNode& in(){return in_;} PatchNode& out(){return out_;}
    Gain& getGain(){return gain_;} Biquad& getEQ(){return eq_;} Panner& getPanner(){return pan_;}
private:Gain gain_;Biquad eq_;Panner pan_;PatchNode in_,out_;
    void ensureBuf(int f){if(outputBuffer.frames()<f||outputBuffer.channels()<2)outputBuffer.allocate(2,f);}
};
}
