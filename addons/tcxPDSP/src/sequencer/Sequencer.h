#pragma once
// Sequencer — Generic timeline-based event sequencer
#include "core/AudioContext.h"
#include "sequencer/Transport.h"
#include "sequencer/EventQueue.h"
#include <vector>
namespace tcx::pdsp {
struct SequenceStep { uint64_t sampleTime; int note; float velocity; float duration; };
class Sequencer {
public:
    void prepare(int sr){sampleRate_=sr;}
    void addStep(uint64_t sampleTime,int note,float vel,float dur){steps_.push_back({sampleTime,note,vel,dur});}
    void clear(){steps_.clear();}
    template<int Q>void process(AudioContext& ctx,Transport& t,int frames,EventQueue<Q>& q){
        uint64_t start=t.currentSample();
        uint64_t end=start+frames;
        for(auto&s:steps_){
            if(s.sampleTime>=start&&s.sampleTime<end){
                SequenceEvent ev;ev.type=EventType::NoteOn;ev.sampleTime=s.sampleTime;
                ev.sampleOffsetInBlock=(int)(s.sampleTime-start);ev.note=s.note;ev.value0=s.velocity;
                q.push(ev);
            }
            uint64_t offTime=s.sampleTime+(uint64_t)(s.duration*sampleRate_);
            if(offTime>=start&&offTime<end){
                SequenceEvent off;off.type=EventType::NoteOff;off.sampleTime=s.sampleTime+(uint64_t)(s.duration*sampleRate_);
                off.sampleOffsetInBlock=(int)(offTime-start);off.note=s.note;
                q.push(off);
            }
        }
    }
private:int sampleRate_=48000;std::vector<SequenceStep> steps_;
};
}
