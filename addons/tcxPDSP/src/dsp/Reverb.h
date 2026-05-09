#pragma once
// FreeVerb-based stereo reverb
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include <vector>
#include <cmath>
namespace tcx::pdsp {
class Reverb : public AudioNode {
public:
    Reverb():in_(this,0),out_(this,0){outputBuffer.allocate(2,256);}
    void prepare(AudioContext& ctx)override{
        outputBuffer.allocate(2,ctx.bufferSize);sr_=(float)ctx.sampleRate;size_.prepare(ctx.sampleRate,20);damp_.prepare(ctx.sampleRate,20);wet_.prepare(ctx.sampleRate,10);
        int maxSize=(int)(sr_*0.5f);//max 500ms
        for(int i=0;i<8;i++){combs_[i].resize(maxSize,0);combsIdx_[i]=0;allpass_[i].resize(maxSize/4,0);apIdx_[i]=0;}
        prepared_=true;
    }
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);copyConnectedInput(in_,f,2);
        float*L=outputBuffer.channel(0),*R=outputBuffer.channel(1);
        float sz=std::max(0.01f,size_.next()),dp=damp_.next(),w=wet_.next();
        float srNorm=sr_/44100.0f;
        for(int i=0;i<f;i++){
            float in=(L[i]+R[i])*0.5f;
            float out=0;
            int delays[8]={1557,1617,1491,1422,1277,1356,1188,1116};
            for(int j=0;j<8;j++){
                int d=(int)(delays[j]*srNorm*sz);
                if(d<1)d=1;if(d>=(int)combs_[j].size())d=(int)combs_[j].size()-1;
                int rp=(combsIdx_[j]-d+(int)combs_[j].size())%(int)combs_[j].size();
                float y=combs_[j][rp];
                combs_[j][combsIdx_[j]]=in+y*dp;
                combsIdx_[j]=(combsIdx_[j]+1)%(int)combs_[j].size();
                out+=y;
            }
            out*=0.25f;
            int apDelays[4]={225,556,441,341};
            for(int j=0;j<4;j++){
                int d=(int)(apDelays[j]*srNorm);
                if(d<1)d=1;if(d>=(int)allpass_[j].size())d=(int)allpass_[j].size()-1;
                int rp=(apIdx_[j]-d+(int)allpass_[j].size())%(int)allpass_[j].size();
                float y=allpass_[j][rp];
                allpass_[j][apIdx_[j]]=out+y*0.5f;
                apIdx_[j]=(apIdx_[j]+1)%(int)allpass_[j].size();
                out=y-out*0.5f;
            }
            L[i]=L[i]*(1-w)+out*w;
            R[i]=R[i]*(1-w)+out*w;
        }
    }
    PatchNode& in(){return in_;} PatchNode& out(){return out_;}
    void setRoomSize(float s){size_.set(s);}   // 0~1
    void setDamping(float d){damp_.set(d);}    // 0~1
    void setWet(float w){wet_.set(w);}          // 0~1
private:
    Parameter size_{0.5f},damp_{0.5f},wet_{0.3f};PatchNode in_,out_;float sr_=48000;
    std::vector<float> combs_[8],allpass_[4];int combsIdx_[8]={},apIdx_[4]={};
    void ensureBuf(int f){if(outputBuffer.frames()<f||outputBuffer.channels()<2)outputBuffer.allocate(2,f);}
};
}
