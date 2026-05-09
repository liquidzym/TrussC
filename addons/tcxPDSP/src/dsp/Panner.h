#pragma once
// Stereo panner — pans mono input to stereo output
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include <cmath>
namespace tcx::pdsp {
class Panner : public AudioNode {
public:
    Panner():in_(this,0),out_(this,0){outputBuffer.allocate(2,256);}
    void prepare(AudioContext& ctx)override{outputBuffer.allocate(2,ctx.bufferSize);pan_.prepare(ctx.sampleRate,10);prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);copyConnectedInput(in_,f,1);
        float*L=outputBuffer.channel(0),*R=outputBuffer.channel(1);
        for(int i=0;i<f;i++){float mono=L[i];float p=pan_.next(),gL=sqrtf(0.5f*(1-p)),gR=sqrtf(0.5f*(1+p));L[i]=mono*gL;R[i]=mono*gR;}
    }
    PatchNode& in(){return in_;} PatchNode& out(){return out_;}
    void setPan(float p){pan_.set(p);}
private:Parameter pan_{0};PatchNode in_,out_;
    void ensureBuf(int f){if(outputBuffer.frames()<f||outputBuffer.channels()<2)outputBuffer.allocate(2,f);}
};
}
