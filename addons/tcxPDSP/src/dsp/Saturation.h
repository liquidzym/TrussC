#pragma once
// Soft saturation / overdrive with drive control
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include <cmath>
namespace tcx::pdsp {
class Saturation : public AudioNode {
public:
    Saturation():in_(this,0),out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{outputBuffer.allocate(1,ctx.bufferSize);drive_.prepare(ctx.sampleRate,5);prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);copyConnectedInput(in_,f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){float d=std::max(0.1f,drive_.next()),x=o[i]*d;o[i]=std::tanh(x)/std::max(0.5f,d*0.5f);}
    }
    PatchNode& in(){return in_;} PatchNode& out(){return out_;}
    void setDrive(float d){drive_.set(d);}
private:Parameter drive_{1.0f};PatchNode in_,out_;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};
}
