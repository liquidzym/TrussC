#pragma once
// Simple drum voice — noise burst + sine body with exponential decay
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include <cmath>
#include <cstdlib>
namespace tcx::pdsp {
class DrumVoice : public AudioNode {
public:
    enum Type { Kick, Snare, Hihat };
    DrumVoice():out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{sr_=(float)ctx.sampleRate;prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_||!triggered_)return;ensureBuf(f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){
            float env=std::exp(-age_*decay_);
            float s=0;
            switch(type_){
            case Kick:{float p=phase_;if(p>1)p-=2;phase_+=freq_/sr_;o[i]=sinf(p*3.14159265f)*env;break;}
            case Snare:{seed_=seed_*1103515245+12345;float n=((seed_>>16)&0x7FFF)/32768.0f;o[i]=(sinf(phase_*6.2831853f)*0.5f+n*0.5f)*env;phase_+=freq_/sr_;break;}
            case Hihat:{seed_=seed_*1103515245+12345;o[i]=((seed_>>16)&0x7FFF)/32768.0f*env*0.5f;break;}
            }
            age_+=1.0f/sr_;
        }
    }
    void trigger(Type t=Kick,float pitch=1.0f){type_=t;triggered_=true;age_=0;phase_=0;
        switch(t){case Kick:freq_=60*pitch;decay_=8.0f;break;case Snare:freq_=180*pitch;decay_=4.0f;break;case Hihat:freq_=0;decay_=10.0f;break;}
    }
    PatchNode& out(){return out_;}
private:Type type_=Kick;PatchNode out_;float sr_=48000,freq_=60,decay_=8,age_=0,phase_=0;bool triggered_=false;uint32_t seed_=12345;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};
}
