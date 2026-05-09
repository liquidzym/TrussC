#include "tcApp.h"
void tcApp::setup(){
    tcx::pdsp::AudioStreamSettings as;as.sampleRate=48000;as.bufferSize=256;as.outputChannels=2;
    stream_.setup(as);synth_.prepare(stream_.context());
    stream_.setCallback([&](float*out,int frm,int ch){
        synth_.process(stream_.context(),frm);
        float*s=synth_.output().channel(0);
        for(int i=0;i<frm;i++){out[i*2]=s[i];out[i*2+1]=s[i];}
    });
    stream_.start();
}
void tcApp::update(){}
void tcApp::draw(){clear(0.1f);setColor(1.f);
    drawBitmapString("FM Synth | Ratio:"+to_string(ratio_).substr(0,3)+" Index:"+to_string(index_).substr(0,3)+" | [A-K]notes [1/2]ratio [3/4]index",12,16);}
void tcApp::keyPressed(int k){
    int notes[]={'A','W','S','E','D','F','T','G','Y','H','U','J','K'};
    int midi[]={60,62,64,65,67,69,71,72,74,76,77,79,81};
    for(int i=0;i<13;i++)if(k==notes[i]){note_=midi[i];synth_.noteOn(tcx::pdsp::math::midiToHz(note_),ratio_,index_);}
    if(k=='1'){ratio_-=0.1f;if(ratio_<0.1f)ratio_=0.1f;}
    if(k=='2'){ratio_+=0.1f;if(ratio_>10)ratio_=10;}
    if(k=='3'){index_-=0.2f;if(index_<0)index_=0;}
    if(k=='4'){index_+=0.2f;if(index_>10)index_=10;}
}
void tcApp::keyReleased(int k){int notes[]={'A','W','S','E','D','F','T','G','Y','H','U','J','K'};for(int i=0;i<13;i++)if(k==notes[i])synth_.noteOff();}
