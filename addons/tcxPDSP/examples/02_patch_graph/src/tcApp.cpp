#include "tcApp.h"
void tcApp::setup(){
    tcx::pdsp::AudioStreamSettings as; as.sampleRate=48000;as.bufferSize=256;as.outputChannels=2;
    stream_.setup(as);
    processor_.setup({stream_.sampleRate(),stream_.bufferSize(),stream_.outputChannels()});
    mixer_.addInput();mixer_.addInput();mixer_.addInput();
    mixer_.setLevel(0,0.1f);mixer_.setLevel(1,0.1f);mixer_.setLevel(2,0.0f);
    osc1_.prepare(stream_.context());osc2_.prepare(stream_.context());
    noise_.prepare(stream_.context());mixer_.prepare(stream_.context());delay_.prepare(stream_.context());
    osc1_.setFrequency(f1_);osc2_.setFrequency(f2_);
    delay_.setDelayTime(0.25f);delay_.setFeedback(0.3f);delay_.setWet(0.3f);
    osc1_.out()>>mixer_.in(0);osc2_.out()>>mixer_.in(1);noise_.out()>>mixer_.in(2);
    mixer_.out()>>delay_.in();delay_.out()>>processor_.out(0);delay_.out()>>processor_.out(1);
    processor_.addNode(&osc1_);processor_.addNode(&osc2_);processor_.addNode(&noise_);
    processor_.addNode(&mixer_);processor_.addNode(&delay_);
    stream_.setCallback([&](float*out,int frm,int ch){
        processor_.process(out,frm,ch);
    });
    stream_.start();
}
void tcApp::update(){}
void tcApp::draw(){clear(0.12f);setColor(1.f);
    drawBitmapString("Osc1:"+to_string((int)f1_)+"Hz Osc2:"+to_string((int)f2_)+"Hz Noise:"+(noiseOn_?"ON":"OFF")+" [1/2]freq [N]noise",12,16);}
void tcApp::keyPressed(int k){
    if(k=='1'){f1_+=5;osc1_.setFrequency(f1_);}if(k=='2'){f2_+=5;osc2_.setFrequency(f2_);}
    if(k=='N'){noiseOn_=!noiseOn_;mixer_.setLevel(2,noiseOn_?0.05f:0.0f);}
}
