#pragma once
// EuclideanSequencer, ProbabilitySequencer, Arpeggiator, PatternChain
#include "core/AudioContext.h"
#include "sequencer/Transport.h"
#include "sequencer/EventQueue.h"
#include <vector>
#include <cstdlib>
namespace tcx::pdsp {

class EuclideanSequencer {
public:
    void prepare(int sr){sampleRate_=sr;currentStep_=0;}
    void setParams(int steps,int pulses,int rotation=0){
        steps_=steps;pulses_=pulses;rotation_=rotation;
        generatePattern();
    }
    template<int Q>void process(AudioContext& ctx,Transport& t,int frames,EventQueue<Q>& q){
        if(!t.isPlaying())return;
        uint64_t spb=t.samplesPerBeat(),sps=spb/4;
        for(int f=0;f<frames;f++){
            uint64_t abs=t.currentSample()+f;
            int step=(abs/sps)%steps_;
            if(step!=currentStep_&&(abs%sps)==0){
                currentStep_=step;
                if(pattern_[step]){SequenceEvent ev;ev.type=EventType::NoteOn;ev.sampleTime=abs;ev.sampleOffsetInBlock=f;ev.note=60;ev.value0=1;q.push(ev);}
                else{SequenceEvent ev;ev.type=EventType::NoteOff;ev.sampleTime=abs;ev.sampleOffsetInBlock=f;q.push(ev);}
            }
        }
    }
private:
    int sampleRate_=48000,steps_=16,pulses_=5,rotation_=0,currentStep_=-1;
    bool pattern_[64]={};
    void generatePattern(){
        for(int i=0;i<64;i++)pattern_[i]=false;
        // Bresenham-style Euclidean rhythm
        if(pulses_>steps_)pulses_=steps_;
        if(pulses_<=0||steps_<=0)return;
        for(int i=0;i<pulses_;i++){
            int pos=(i*steps_+pulses_/2)/pulses_;
            pattern_[(pos+rotation_)%steps_]=true;
        }
    }
};

class ProbabilitySequencer {
public:
    struct ProbStep {bool active=false;int note=60;float vel=1,prob=1,nextProb=0.5f;};
    void prepare(int sr){sampleRate_=sr;currentStep_=-1;steps_.resize(16);}
    void setSteps(int n){steps_.resize(n);}
    void setStep(int i,bool active,int note,float vel,float prob=1,float nextProb=0.5f){
        if(i<0||i>=(int)steps_.size())return;steps_[i]={active,note,vel,prob,nextProb};}
    template<int Q>void process(AudioContext& ctx,Transport& t,int frames,EventQueue<Q>& q){
        if(!t.isPlaying())return;
        uint64_t spb=t.samplesPerBeat(),sps=spb/4;
        for(int f=0;f<frames;f++){
            uint64_t abs=t.currentSample()+f;int step=(int)((abs/sps)%steps_.size());
            if(step!=currentStep_&&(abs%sps)==0){
                currentStep_=step;auto&s=steps_[step];
                if(s.active&&(float)rand()/2147483647.0f<s.prob){
                    // Maybe follow with conditional step
                    if((float)rand()/2147483647.0f<s.nextProb&&step+1<(int)steps_.size()){
                        auto&ns=steps_[step+1];if(ns.active){
                            SequenceEvent ev;ev.type=EventType::NoteOn;ev.sampleTime=abs+sps;ev.sampleOffsetInBlock=f+(int)sps;ev.note=ns.note;ev.value0=ns.vel;q.push(ev);
                        }
                    }
                    SequenceEvent ev;ev.type=EventType::NoteOn;ev.sampleTime=abs;ev.sampleOffsetInBlock=f;ev.note=s.note;ev.value0=s.vel;q.push(ev);
                }else{SequenceEvent ev;ev.type=EventType::NoteOff;ev.sampleTime=abs;ev.sampleOffsetInBlock=f;q.push(ev);}
            }
        }
    }
private:int sampleRate_=48000,currentStep_=-1;std::vector<ProbStep> steps_;
};

class Arpeggiator {
public:
    enum Mode { Up,Down,UpDown,Random };
    void prepare(int sr){sampleRate_=sr;}
    void setNotes(const std::vector<int>& notes){notes_=notes;idx_=0;dir_=1;}
    void setMode(Mode m){mode_=m;}
    void setOctaves(int o){octaves_=o;}
    template<int Q>void process(AudioContext& ctx,Transport& t,int frames,EventQueue<Q>& q){
        if(!t.isPlaying()||notes_.empty())return;
        uint64_t spb=t.samplesPerBeat(),sps=spb/4;
        for(int f=0;f<frames;f++){
            uint64_t abs=t.currentSample()+f;
            if((abs%sps)==0&&abs!=lastSample_){
                lastSample_=abs;
                int nidx=idx_;int totalNotes=(int)notes_.size()*octaves_;
                switch(mode_){
                case Up:idx_++;if(idx_>=totalNotes)idx_=0;break;
                case Down:idx_--;if(idx_<0)idx_=totalNotes-1;break;
                case UpDown:idx_+=dir_;if(idx_>=totalNotes-1||idx_<=0)dir_=-dir_;break;
                case Random:idx_=rand()%totalNotes;break;
                }
                int oct=nidx/(int)notes_.size(),ni=nidx%(int)notes_.size();
                int note=notes_[ni]+oct*12;
                SequenceEvent ev;ev.type=EventType::NoteOn;ev.sampleTime=abs;ev.sampleOffsetInBlock=f;ev.note=note;ev.value0=1;q.push(ev);
            }
        }
    }
private:int sampleRate_=48000,idx_=0,dir_=1,octaves_=1;Mode mode_=Up;std::vector<int> notes_;uint64_t lastSample_=~0ULL;
};

class PatternChain {
public:
    struct Entry {int patternIdx;int repeatCount;};
    void add(int patternIdx,int repeat=1){entries_.push_back({patternIdx,repeat});}
    void clear(){entries_.clear();}
    int currentPattern()const{return currentEntry_>=0&&currentEntry_<(int)entries_.size()?entries_[currentEntry_].patternIdx:-1;}
    int currentRepeat()const{return currentRepeat_;}
    void advance(){
        if(entries_.empty())return;
        currentRepeat_++;
        if(currentRepeat_>=entries_[currentEntry_].repeatCount){
            currentRepeat_=0;currentEntry_=(currentEntry_+1)%(int)entries_.size();
        }
    }
    void reset(){currentEntry_=0;currentRepeat_=0;}
private:std::vector<Entry> entries_;int currentEntry_=0,currentRepeat_=0;
};

}
