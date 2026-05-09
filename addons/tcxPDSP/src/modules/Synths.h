#pragma once
// FMSynth — 2-operator FM, WavetableOsc, DrumSynth, VoiceAllocator
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "dsp/Oscillator.h"
#include "dsp/ADSR.h"
#include <vector>
#include <cmath>
namespace tcx::pdsp {

class FMSynth : public AudioNode {
public:
    FMSynth():out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{sr_=(float)ctx.sampleRate;carrier_.setSampleRate(sr_);modulator_.setSampleRate(sr_);
        carrier_.setAttack(0.02f);carrier_.setDecay(0.1f);carrier_.setSustain(0.5f);carrier_.setRelease(0.2f);
        modulator_.setAttack(0.01f);modulator_.setDecay(0.3f);modulator_.setSustain(0);modulator_.setRelease(0.1f);prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){
            float modEnv=modulator_.process();
            float mod=sinf(modPhase_*6.2831853f)*modEnv*modIndex_*carrierFreq_;
            float carEnv=carrier_.process();
            float car=sinf(carPhase_*6.2831853f)*carEnv;
            modPhase_+=modFreq_/sr_;if(modPhase_>=1)modPhase_-=1;
            carPhase_+=(carrierFreq_+mod)/sr_;if(carPhase_>=1)carPhase_-=1;
            o[i]=car*0.3f;
        }
    }
    PatchNode& out(){return out_;}
    void noteOn(float carrierHz,float modRatio=2,float index=1){
        carrierFreq_=carrierHz;modFreq_=carrierHz*modRatio;modIndex_=index;
        carrier_.noteOn();modulator_.noteOn();}
    void noteOff(){carrier_.noteOff();modulator_.noteOff();}
    void setAttack(float s){carrier_.setAttack(s);}
    void setDecay(float s){carrier_.setDecay(s);}
    void setSustain(float l){carrier_.setSustain(l);}
    void setRelease(float s){carrier_.setRelease(s);}
private:ADSR carrier_,modulator_;PatchNode out_;float sr_=48000,carrierFreq_=440,modFreq_=880,modIndex_=1,carPhase_=0,modPhase_=0;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};

class WavetableOsc : public AudioNode {
public:
    WavetableOsc():out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{sr_=(float)ctx.sampleRate;freq_.prepare(ctx.sampleRate,5);pos_.prepare(ctx.sampleRate,10);
        // Generate default sine table
        table_.resize(2048);for(int i=0;i<2048;i++)table_[i]=sinf(i/2048.0f*6.2831853f);prepared_=true;}
    void setTable(const float* data,int size){table_.assign(data,data+size);}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);float*o=outputBuffer.channel(0);
        int tsize=(int)table_.size();
        for(int i=0;i<f;i++){float fr=freq_.next(),inc=fr*tsize/sr_;phase_+=inc;if(phase_>=tsize)phase_-=tsize;
            int i0=(int)phase_,i1=(i0+1)%tsize;float frac=phase_-i0;
            float p=pos_.next()*(tsize-2);int pi0=(int)p,pi1=pi0+1;float pfrac=p-pi0;
            float v0=table_[i0]*(1-frac)+table_[i1]*frac;
            float v1=table_[(i0+pi1)%tsize]*(1-frac)+table_[(i1+pi1)%tsize]*frac;
            o[i]=v0*(1-pfrac)+v1*pfrac;}
    }
    PatchNode& out(){return out_;}
    void setFrequency(float hz){freq_.set(hz);}
    void setPosition(float p){pos_.set(p);} // sweep through wavetable
private:Parameter freq_{440},pos_{0};PatchNode out_;float sr_=48000,phase_=0;std::vector<float> table_;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};

class VoiceAllocator {
public:
    struct Voice { int note=-1;float vel=0;bool active=false;uint64_t age=0; };
    VoiceAllocator(int maxVoices=8):voices_(maxVoices){}
    Voice* allocate(int note){
        Voice* target=nullptr;uint64_t oldest=0;
        for(auto&v:voices_){if(!v.active){target=&v;break;}if(v.note==note){target=&v;break;}if(v.age>=oldest){oldest=v.age;target=&v;}}
        if(!target)return nullptr;
        target->note=note;target->active=true;target->age=0;
        return target;
    }
    void release(int note){for(auto&v:voices_)if(v.note==note&&v.active)v.active=false;}
    void ageAll(){for(auto&v:voices_)if(v.active)v.age++;}
    int activeCount()const{int n=0;for(auto&v:voices_)if(v.active)n++;return n;}
private:std::vector<Voice> voices_;
};

}
