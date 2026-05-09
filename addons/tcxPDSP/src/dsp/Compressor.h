#pragma once
// Compressor — RMS-based dynamics compressor
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include <cmath>
namespace tcx::pdsp {
class Compressor : public AudioNode {
public:
    Compressor():in_(this,0),out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{outputBuffer.allocate(1,ctx.bufferSize);sr_=(float)ctx.sampleRate;thresh_.prepare(ctx.sampleRate,10);ratio_.prepare(ctx.sampleRate,10);attack_.prepare(ctx.sampleRate,10);release_.prepare(ctx.sampleRate,10);makeup_.prepare(ctx.sampleRate,10);prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);copyConnectedInput(in_,f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){
            float th=thresh_.next(),ra=ratio_.next(),at=attack_.next(),re=release_.next(),mk=makeup_.next();
            float atC=1-expf(-1/(at*sr_*0.001f+1)),reC=1-expf(-1/(re*sr_*0.001f+1));
            float level=fabsf(o[i]);if(level<0.0001f)level=0.0001f;
            float db=20*log10f(level),over=db-th;
            float gainDB=over>0?over*(1/ra-1):0;
            float targetGain=powf(10,gainDB/20);
            if(targetGain<env_)env_+=atC*(targetGain-env_);else env_+=reC*(targetGain-env_);
            o[i]*=env_*powf(10,mk/20);
        }
    }
    PatchNode& in(){return in_;}PatchNode& out(){return out_;}
    void setThreshold(float db){thresh_.set(db);}void setRatio(float r){ratio_.set(r);}
    void setAttack(float ms){attack_.set(ms);}void setRelease(float ms){release_.set(ms);}
    void setMakeup(float db){makeup_.set(db);}
private:Parameter thresh_{-20},ratio_{4},attack_{10},release_{100},makeup_{0};PatchNode in_,out_;float sr_=48000,env_=1;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};
}
