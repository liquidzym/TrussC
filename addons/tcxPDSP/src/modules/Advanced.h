#pragma once
// GranularSynth, PhysicalModel (Karplus-Strong), SamplePool, DrumMachine
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include "modules/DrumVoice.h"
#include "sequencer/Transport.h"
#include "sequencer/EventQueue.h"
#include <vector>
#include <map>
#include <cmath>
#include <cstdlib>
namespace tcx::pdsp {

class GranularSynth : public AudioNode {
public:
    GranularSynth():out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{sr_=(float)ctx.sampleRate;density_.prepare(ctx.sampleRate,10);size_.prepare(ctx.sampleRate,10);pitch_.prepare(ctx.sampleRate,10);prepared_=true;}
    void loadSample(const float* data,int count){sample_.assign(data,data+count);}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_||sample_.empty())return;ensureBuf(f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){o[i]=0;float d=density_.next(),sz=size_.next(),pt=pitch_.next();
            if((float)rand()/2147483647.0f<d*sr_*0.0001f){
                int start=rand()%(int)(sample_.size()*0.9f);
                grains_.push_back({start,0.0f,sz,pt});
            }
            for(auto&g:grains_){
                if(g.pos<g.size&&g.start+(int)g.pos<(int)sample_.size()){
                    float env=1-g.pos/g.size;
                    float s=sample_[g.start+(int)g.pos]*env*0.3f;
                    o[i]+=s;g.pos+=g.pitch;
                }
            }
            grains_.erase(std::remove_if(grains_.begin(),grains_.end(),[](auto&g){return g.pos>=g.size;}),grains_.end());
        }
    }
    PatchNode& out(){return out_;}
    void setDensity(float d){density_.set(d);}void setGrainSize(float s){size_.set(s);}void setPitch(float p){pitch_.set(p);}
private:struct Grain{int start;float pos,size,pitch;};std::vector<Grain> grains_;std::vector<float> sample_;
    Parameter density_{10},size_{0.1f},pitch_{1};PatchNode out_;float sr_=48000;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};

class PhysicalModel : public AudioNode {
public:
    PhysicalModel():out_(this,0){outputBuffer.allocate(1,256);}
    void prepare(AudioContext& ctx)override{sr_=(float)ctx.sampleRate;freq_.prepare(ctx.sampleRate,10);decay_.prepare(ctx.sampleRate,10);
        int maxD=(int)(sr_*0.1f);delay_.resize(maxD,0);prepared_=true;}
    void process(AudioContext& ctx,int f)override{
        if(!prepared_||!active_)return;ensureBuf(f);float*o=outputBuffer.channel(0);
        for(int i=0;i<f;i++){float fr=freq_.next(),dc=decay_.next();
            int d=(int)(sr_/fr);if(d<1)d=1;if(d>=(int)delay_.size())d=(int)delay_.size()-1;
            int rp=(writePos_-d+(int)delay_.size())%(int)delay_.size();
            float y=delay_[rp];delay_[writePos_]=y*dc;writePos_=(writePos_+1)%(int)delay_.size();
            o[i]=y;}
    }
    void pluck(float amplitude=1){
        int len=(int)delay_.size()/4;
        for(int i=0;i<len;i++)delay_[i]=(rand()%2000-1000)/1000.0f*amplitude;
        // Advance writePos_ past pluck so read position lands in the excitation
        writePos_=len;
    }
    PatchNode& out(){return out_;}
    void setFrequency(float hz){freq_.set(hz);}void setDecay(float d){decay_.set(d);}
private:Parameter freq_{220},decay_{0.99f};PatchNode out_;float sr_=48000;std::vector<float> delay_;int writePos_=0;
    void ensureBuf(int f){if(outputBuffer.frames()<f)outputBuffer.allocate(1,f);}
};

class SamplePool {
public:
    struct Sample {std::vector<float> data;int sampleRate=48000;int rootNote=60;};
    void addSample(int note,const float* data,int count,int sr=48000){
        Sample s;s.data.assign(data,data+count);s.sampleRate=sr;s.rootNote=note;samples_[note]=s;}
    const Sample* getSample(int note)const{auto it=samples_.find(note);return it!=samples_.end()?&it->second:nullptr;}
    int size()const{return (int)samples_.size();}
    void clear(){samples_.clear();}
private:std::map<int,Sample> samples_;
};

class DrumMachine {
public:
    DrumMachine(){for(int i=0;i<8;i++){auto&t=track_[i];t.steps.resize(16);}}
    void prepare(AudioContext& ctx){
        for(int i=0;i<8;i++){voices_[i].prepare(ctx);voices_[i].setActive(false);}}
    void setStep(int track,int step,bool active,float vel=1){
        if(track<0||track>7||step<0||step>15)return;track_[track].steps[step]={active,vel};}
    void process(AudioContext& ctx,Transport& t,int frames,EventQueue<256>& q){
        if(!t.isPlaying())return;
        uint64_t spb=t.samplesPerBeat(),sps=spb/4;
        for(int f=0;f<frames;f++){
            uint64_t abs=t.currentSample()+f;int step=(abs/sps)%16;
            if(step!=currentStep_&&(abs%sps)==0){
                currentStep_=step;
                for(int tr=0;tr<8;tr++){
                    auto&s=track_[tr].steps[step];
                    if(s.active){SequenceEvent ev;ev.type=EventType::Trigger;ev.sampleTime=abs;ev.sampleOffsetInBlock=f;ev.targetId=tr;ev.value0=s.vel;q.push(ev);}
                }
            }
        }
    }
    int getCurrentStep()const{return currentStep_;}
    void triggerVoice(int idx,float vel,DrumVoice::Type type=DrumVoice::Kick){
        if(idx<0||idx>7)return;voices_[idx].trigger(type,vel);}
private:
    struct TrackStep{bool active=false;float vel=1;};
    struct Track{std::vector<TrackStep> steps;};
    Track track_[8];DrumVoice voices_[8];int currentStep_=-1;
};

}
