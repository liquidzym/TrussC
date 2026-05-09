#pragma once
// CombFilter, WaveShaper, BitCrusher, AllPass
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include <vector>
#include <cmath>
namespace tcx::pdsp {

class CombFilter : public AudioNode {
public:
    CombFilter():in_(this,0),out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{outputBuffer.allocate(1,ctx.bufferSize);sr_=(float)ctx.sampleRate;freq_.prepare(ctx.sampleRate,10);fb_.prepare(ctx.sampleRate,10);wet_.prepare(ctx.sampleRate,10);
        delay_.resize((int)sr_,0);prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);copyConnectedInput(in_,f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){float fr=freq_.next(),fb=fb_.next(),w=wet_.next();
            int d=(int)(sr_/fr);if(d<1)d=1;if(d>=(int)delay_.size())d=(int)delay_.size()-1;
            int rp=(writePos_-d+(int)delay_.size())%(int)delay_.size();
            float y=delay_[rp];delay_[writePos_]=o[i]+y*fb;writePos_=(writePos_+1)%(int)delay_.size();
            o[i]=o[i]*(1-w)+y*w;}
    }
    PatchNode& in(){return in_;}PatchNode& out(){return out_;}
    void setFrequency(float hz){freq_.set(hz);}void setFeedback(float f){fb_.set(f);}void setWet(float w){wet_.set(w);}
private:Parameter freq_{440},fb_{0.5f},wet_{0.5f};PatchNode in_,out_;float sr_=48000;std::vector<float> delay_;int writePos_=0;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};

class WaveShaper : public AudioNode {
public:
    WaveShaper():in_(this,0),out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{outputBuffer.allocate(1,ctx.bufferSize);drive_.prepare(ctx.sampleRate,5);prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);copyConnectedInput(in_,f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){float d=drive_.next(),x=o[i]*d;o[i]=tanhf(x);}
    }
    PatchNode& in(){return in_;}PatchNode& out(){return out_;}
    void setDrive(float d){drive_.set(d);}
private:Parameter drive_{1};PatchNode in_,out_;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};

class BitCrusher : public AudioNode {
public:
    BitCrusher():in_(this,0),out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{outputBuffer.allocate(1,ctx.bufferSize);bits_.prepare(ctx.sampleRate,10);rate_.prepare(ctx.sampleRate,10);prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);copyConnectedInput(in_,f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){float b=bits_.next(),r=rate_.next();
            int levels=(int)powf(2,b-1);float q=1.0f/levels;
            o[i]=roundf(o[i]*levels)*q;
            if(r<1){int hold=(int)(1/r);if(cnt_++>=hold){cnt_=0;last_=o[i];}o[i]=last_;}
        }
    }
    PatchNode& in(){return in_;}PatchNode& out(){return out_;}
    void setBits(float b){bits_.set(b);}void setSampleRate(float r){rate_.set(r);}
private:Parameter bits_{8},rate_{1};PatchNode in_,out_;int cnt_=0;float last_=0;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};

class AllPass : public AudioNode {
public:
    AllPass():in_(this,0),out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{outputBuffer.allocate(1,ctx.bufferSize);sr_=(float)ctx.sampleRate;freq_.prepare(ctx.sampleRate,10);prepared_=true;
        delay_.resize((int)sr_,0);}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);copyConnectedInput(in_,f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){float fr=freq_.next();int d=(int)(sr_/fr);if(d<1)d=1;if(d>=(int)delay_.size())d=(int)delay_.size()-1;
            int rp=(writePos_-d+(int)delay_.size())%(int)delay_.size();
            float y=delay_[rp];delay_[writePos_]=o[i]+y*0.5f;writePos_=(writePos_+1)%(int)delay_.size();
            o[i]=y-o[i]*0.5f;}
    }
    PatchNode& in(){return in_;}PatchNode& out(){return out_;}
    void setFrequency(float hz){freq_.set(hz);}
private:Parameter freq_{1000};PatchNode in_,out_;float sr_=48000;std::vector<float> delay_;int writePos_=0;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};

}
