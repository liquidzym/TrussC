#pragma once
// Pattern — Named sequence container with bar/beat structure
#include "core/AudioContext.h"
#include "sequencer/Transport.h"
#include <vector>
#include <string>
namespace tcx::pdsp {
struct PatternNote { int note; float velocity; float startBeat; float durationBeats; };
class Pattern {
public:
    void setName(const std::string& n){name_=n;}
    const std::string& name()const{return name_;}
    void addNote(int note,float vel,float startBeat,float durBeats=-1){
        if(durBeats<0)durBeats=1.0f;notes_.push_back({note,vel,startBeat,durBeats});
    }
    void clear(){notes_.clear();}
    int barCount()const{return bars_;}
    void setBars(int b){bars_=b;}
    // Convert pattern to sample-time sequence events
    template<int Q>void schedule(Transport& t,EventQueue<Q>& q,uint64_t barStartSample){
        double bpm=t.getBpm();if(bpm<=0)return;
        double spb=60.0*t.sampleRate()/bpm;
        for(auto&n:notes_){
            uint64_t t0=barStartSample+(uint64_t)(n.startBeat*spb);
            SequenceEvent ev;ev.type=EventType::NoteOn;ev.sampleTime=t0;ev.note=n.note;ev.value0=n.velocity;
            q.push(ev);
            uint64_t t1=barStartSample+(uint64_t)((n.startBeat+n.durationBeats)*spb);
            SequenceEvent off;off.type=EventType::NoteOff;off.sampleTime=t1;off.note=n.note;
            q.push(off);
        }
    }
private:std::string name_;std::vector<PatternNote> notes_;int bars_=1;
};
}
