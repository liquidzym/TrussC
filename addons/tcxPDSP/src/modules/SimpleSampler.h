#pragma once
// Simple buffer-based sampler — loads float samples, plays with pitch/loop
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include <vector>
#include <cstring>
namespace tcx::pdsp {
class SimpleSampler : public AudioNode {
public:
    SimpleSampler():out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{sr_=(float)ctx.sampleRate;prepared_=true;}
    void loadSamples(const float* data,int count){samples_.assign(data,data+count);pos_=0;playing_=false;}
    void noteOn(){pos_=0;playing_=true;}
    void noteOff(){playing_=false;}
    void setLoop(bool l){loop_=l;}
    void setSpeed(float s){speed_=s;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_||!playing_||samples_.empty())return;ensureBuf(f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){
            int i0=(int)pos_,i1=i0+1;float frac=pos_-i0;
            if(i1>=(int)samples_.size()){if(loop_){i0=0;i1=1;pos_=0;frac=0;}else{playing_=false;o[i]=0;continue;}}
            o[i]=samples_[i0]*(1-frac)+samples_[i1]*frac;
            pos_+=speed_;
        }
    }
    PatchNode& out(){return out_;}
    bool isPlaying()const{return playing_;}
private:std::vector<float> samples_;PatchNode out_;float sr_=48000,pos_=0,speed_=1;bool playing_=false,loop_=false;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};
}
