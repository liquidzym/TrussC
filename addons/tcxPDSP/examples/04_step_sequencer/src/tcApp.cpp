#include "tcApp.h"
#include <array>
#include <algorithm>
void tcApp::setup(){
    tcx::pdsp::AudioStreamSettings as; as.sampleRate=48000;as.bufferSize=256;as.outputChannels=2;
    stream_.setup(as);
    synth_.prepare(stream_.context());
    synth_.setCutoff(3000);synth_.setResonance(0.3f);
    synth_.setAttack(0.01f);synth_.setDecay(0.05f);synth_.setSustain(0.4f);synth_.setRelease(0.1f);

    transport_.prepare(48000);transport_.setBpm(120);
    seq_.prepare(48000);
    // Program a simple pattern
    int notes[]={60,64,67,72,67,64,60,0,62,65,69,74,69,65,62,0};
    for(int i=0;i<16;i++)seq_.setStep(i,notes[i]>0,notes[i]>0?notes[i]:60,0.8f);
    transport_.play();

    stream_.setCallback([&](float*out,int frm,int ch){
        auto&ctx=stream_.context();
        seq_.process(ctx,transport_,frm,queue_);
        std::array<tcx::pdsp::SequenceEvent,64> events;
        int eventCount=0;
        tcx::pdsp::SequenceEvent ev;
        while(eventCount<(int)events.size()&&queue_.pop(ev)){
            ev.sampleOffsetInBlock=std::clamp(ev.sampleOffsetInBlock,0,frm);
            events[eventCount++]=ev;
        }
        int cursor=0,eventIndex=0;
        auto renderSegment=[&](int start,int count){
            if(count<=0)return;
            synth_.process(ctx,count);
            float*s=synth_.output().channel(0);
            for(int i=0;i<count;i++){float v=s[i];out[(start+i)*ch]=v;if(ch>1)out[(start+i)*ch+1]=v;}
        };
        while(eventIndex<eventCount){
            int offset=events[eventIndex].sampleOffsetInBlock;
            renderSegment(cursor,offset-cursor);
            cursor=offset;
            while(eventIndex<eventCount&&events[eventIndex].sampleOffsetInBlock==offset){
                auto&e=events[eventIndex++];
                if(e.type==tcx::pdsp::EventType::NoteOn)synth_.noteOn(e.note,e.value0);
                else if(e.type==tcx::pdsp::EventType::NoteOff)synth_.noteOff();
            }
        }
        renderSegment(cursor,frm-cursor);
        transport_.advance(frm);
    });
    stream_.start();
}
void tcApp::update(){step_=seq_.getCurrentStep();}
void tcApp::draw(){clear(0.12f);setColor(1.f);
    string bar;for(int i=0;i<16;i++)bar+=(i==step_?"[#]":(seq_.getSteps()[i].active?"[.]":"[ ]"));
    drawBitmapString("Step: "+bar+" 120BPM [Space]play/stop",12,16);}
void tcApp::keyPressed(int k){
    if(k==KEY_SPACE){if(transport_.isPlaying())transport_.stop();else transport_.play();}
}
