#pragma once
// FormantFilter, Phaser, Tremolo, RingMod, Crossfader
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include <cmath>
#include <vector>
namespace tcx::pdsp {

class FormantFilter : public AudioNode {
public:
    FormantFilter():in_(this,0),out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{outputBuffer.allocate(1,ctx.bufferSize);sr_=(float)ctx.sampleRate;vowel_.prepare(ctx.sampleRate,20);shift_.prepare(ctx.sampleRate,20);prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);copyConnectedInput(in_,f);float*o=outputBuffer.channel(0);
        // Simplified 3-formant model (a, e, i, o, u)
        float f1[]={730,530,270,480,300},f2[]={1090,1840,2290,800,870},f3[]={2440,2480,3010,2450,2240};
        float bw=100;
        for(int i=0;i<f;i++){float v=vowel_.next(),sh=shift_.next();
            int idx=(int)(v*4.999f);float out=0,x=o[i];
            for(int j=0;j<3;j++){
                float ff=(j==0?f1:j==1?f2:f3)[idx]*sh;
                float r=1-expf(-6.2831853f*bw/sr_);
                float c=expf(-6.2831853f*ff/sr_);
                float y=r*x+(1-r)*c*state_[j];state_[j]=y;
                out+=y*(j==0?1:j==1?0.5f:0.25f);
            }
            o[i]=out*0.5f;
        }
    }
    PatchNode& in(){return in_;}PatchNode& out(){return out_;}
    void setVowel(float v){vowel_.set(v);}void setShift(float s){shift_.set(s);}
private:Parameter vowel_{2},shift_{1};PatchNode in_,out_;float sr_=48000,state_[3]={};
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};

class Phaser : public AudioNode {
public:
    Phaser():in_(this,0),out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{outputBuffer.allocate(1,ctx.bufferSize);sr_=(float)ctx.sampleRate;rate_.prepare(ctx.sampleRate,10);depth_.prepare(ctx.sampleRate,10);fb_.prepare(ctx.sampleRate,10);
        for(int i=0;i<4;i++){apBuf_[i].resize(256,0);apIdx_[i]=0;}prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);copyConnectedInput(in_,f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){float r=rate_.next(),d=depth_.next(),fb=fb_.next();
            float mod=sinf(phase_*6.2831853f)*d*100+200;phase_+=r/sr_;if(phase_>=1)phase_-=1;
            int delay=(int)(sr_/mod);if(delay<1)delay=1;if(delay>255)delay=255;
            float x=o[i]+lastOut_*fb;
            for(int j=0;j<4;j++){
                int rp=(apIdx_[j]-delay+(int)apBuf_[j].size())%(int)apBuf_[j].size();
                float y=apBuf_[j][rp];apBuf_[j][apIdx_[j]]=x+y*0.5f;apIdx_[j]=(apIdx_[j]+1)%(int)apBuf_[j].size();
                x=y-x*0.5f;
            }
            lastOut_=x;o[i]=o[i]+x*0.7f;
        }
    }
    PatchNode& in(){return in_;}PatchNode& out(){return out_;}
    void setRate(float hz){rate_.set(hz);}void setDepth(float d){depth_.set(d);}void setFeedback(float f){fb_.set(f);}
private:Parameter rate_{0.3f},depth_{0.7f},fb_{0.5f};PatchNode in_,out_;float sr_=48000,phase_=0,lastOut_=0;
    std::vector<float> apBuf_[4];int apIdx_[4];
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};

class Tremolo : public AudioNode {
public:
    Tremolo():in_(this,0),out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{outputBuffer.allocate(1,ctx.bufferSize);sr_=(float)ctx.sampleRate;rate_.prepare(ctx.sampleRate,10);depth_.prepare(ctx.sampleRate,10);prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);copyConnectedInput(in_,f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){float r=rate_.next(),d=depth_.next();
            float mod=1-d*0.5f*(1+sinf(phase_*6.2831853f));phase_+=r/sr_;if(phase_>=1)phase_-=1;
            o[i]*=mod;}
    }
    PatchNode& in(){return in_;}PatchNode& out(){return out_;}
    void setRate(float hz){rate_.set(hz);}void setDepth(float d){depth_.set(d);}
private:Parameter rate_{4},depth_{0.8f};PatchNode in_,out_;float sr_=48000,phase_=0;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};

class RingMod : public AudioNode {
public:
    RingMod():in_(this,0),out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{outputBuffer.allocate(1,ctx.bufferSize);sr_=(float)ctx.sampleRate;freq_.prepare(ctx.sampleRate,10);prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);copyConnectedInput(in_,f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){float fr=freq_.next(),inc=fr/sr_;o[i]*=sinf(phase_*6.2831853f);phase_+=inc;if(phase_>=1)phase_-=1;}
    }
    PatchNode& in(){return in_;}PatchNode& out(){return out_;}
    void setFrequency(float hz){freq_.set(hz);}
private:Parameter freq_{440};PatchNode in_,out_;float sr_=48000,phase_=0;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};

class Crossfader : public AudioNode {
public:
    Crossfader():out_{this,0}{outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{outputBuffer.allocate(1,ctx.bufferSize);xfade_.prepare(ctx.sampleRate,10);prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);copyConnectedInput(inA_,f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){float x=xfade_.next();o[i]=o[i]*x+bufB_[i]*(1-x);}
    }
    PatchNode& inA(){return inA_;} PatchNode& out(){return out_;}
    void setFade(float v){xfade_.set(v);}//0=all B,1=all A
    float* inputB(){return bufB_;}//direct B input
private:Parameter xfade_{0.5f};PatchNode inA_{this,0},out_{this,0};float bufB_[4096]={};
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};

}
