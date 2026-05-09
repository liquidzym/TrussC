#include "tcApp.h"
void tcApp::setup(){
    tcx::pdsp::AudioStreamSettings as;as.sampleRate=48000;as.bufferSize=256;as.outputChannels=2;
    stream_.setup(as);kick_.prepare(stream_.context());snare_.prepare(stream_.context());hat_.prepare(stream_.context());
    transport_.prepare(48000);transport_.setBpm(130);transport_.play();
    kickSeq_.prepare(48000);kickSeq_.setParams(16,5);
    snareSeq_.prepare(48000);snareSeq_.setParams(16,3);
    hatSeq_.prepare(48000);hatSeq_.setParams(16,7);
    stream_.setCallback([&](float*out,int frm,int ch){
        auto&ctx=stream_.context();
        kickSeq_.process(ctx,transport_,frm,kickQ_);snareSeq_.process(ctx,transport_,frm,snareQ_);hatSeq_.process(ctx,transport_,frm,hatQ_);
        tcx::pdsp::SequenceEvent ev;
        while(kickQ_.pop(ev)){if(ev.type==tcx::pdsp::EventType::NoteOn)kick_.trigger(tcx::pdsp::DrumVoice::Kick);}
        while(snareQ_.pop(ev)){if(ev.type==tcx::pdsp::EventType::NoteOn)snare_.trigger(tcx::pdsp::DrumVoice::Snare);}
        while(hatQ_.pop(ev)){if(ev.type==tcx::pdsp::EventType::NoteOn)hat_.trigger(tcx::pdsp::DrumVoice::Hihat);}
        transport_.advance(frm);
        kick_.process(ctx,frm);snare_.process(ctx,frm);hat_.process(ctx,frm);
        float*k=kick_.output().channel(0),*s=snare_.output().channel(0),*h=hat_.output().channel(0);
        for(int i=0;i<frm;i++){float mix=(k[i]+s[i]+h[i])*0.5f;out[i*2]=mix;out[i*2+1]=mix;}
    });
    stream_.start();
}
void tcApp::update(){}
void tcApp::draw(){clear(0.1f);setColor(1.f);
    drawBitmapString("Euclidean Drum | Kick:5/16 Snare:3/16 Hihat:7/16 | 130BPM [Space]stop",12,16);}
void tcApp::keyPressed(int k){if(k==KEY_SPACE){if(transport_.isPlaying())transport_.stop();else transport_.play();}}
