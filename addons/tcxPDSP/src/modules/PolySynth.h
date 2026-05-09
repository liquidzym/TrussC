#pragma once
// PolySynth — Multi-voice subtractive synthesizer with voice stealing
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "dsp/SineOsc.h"
#include "dsp/FilterSVF.h"
#include "dsp/ADSR.h"
#include <vector>
#include <cmath>
namespace tcx::pdsp {
class PolySynth : public AudioNode {
public:
    struct Voice { SineOsc osc; ADSR env; FilterSVF filter; int note=-1; bool active=false; uint64_t age=0; };
    PolySynth(int voices=8):voiceCount_(voices),out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{
        sr_=(float)ctx.sampleRate;voices_.resize(voiceCount_);
        for(int i=0;i<voiceCount_;i++)voices_[i]=std::make_unique<Voice>();
        for(auto&v:voices_){v->osc.prepare(ctx);v->filter.prepare(ctx);v->env.setSampleRate(sr_);}
        prepared_=true;
    }
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++)o[i]=0;
        for(auto&v:voices_){
            if(!v->active)continue;
            v->osc.process(ctx,f);
            // Route osc output → filter input (filter processes in-place)
            float*oscO=v->osc.output().channel(0);
            float*filtBuf=v->filter.output().channel(0);
            for(int i=0;i<f;i++)filtBuf[i]=oscO[i];
            v->filter.process(ctx,f);
            for(int i=0;i<f;i++){float env=v->env.process();o[i]+=filtBuf[i]*env;v->age++;}
            if(v->env.getStage()==ADSR::Stage::Idle)v->active=false;
        }
    }
    void noteOn(int note,float vel=1.0f){
        Voice*target=nullptr;uint64_t oldest=~0ULL;
        for(auto&v:voices_){if(!v->active){target=v.get();break;}if(v->note==note){target=v.get();break;}if(v->age>oldest){oldest=v->age;target=v.get();}}
        if(!target)return;
        target->note=note;target->active=true;target->age=0;
        float f=440.0f*std::pow(2.0f,(note-69)/12.0f);
        target->osc.setFrequency(f);target->env.noteOn();
    }
    void noteOff(int note){for(auto&v:voices_)if(v->note==note&&v->active)v->env.noteOff();}
    void setCutoff(float hz){for(auto&v:voices_)v->filter.setCutoff(hz);}
    void setResonance(float q){for(auto&v:voices_)v->filter.setResonance(q);}
    void setAttack(float s){for(auto&v:voices_)v->env.setAttack(s);}
    void setDecay(float s){for(auto&v:voices_)v->env.setDecay(s);}
    void setSustain(float l){for(auto&v:voices_)v->env.setSustain(l);}
    void setRelease(float s){for(auto&v:voices_)v->env.setRelease(s);}
    PatchNode& out(){return out_;}
private:
    int voiceCount_=8;std::vector<std::unique_ptr<Voice>> voices_;PatchNode out_;float sr_=48000;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};
}
