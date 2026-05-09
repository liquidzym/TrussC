#include "tcApp.h"
void tcApp::setup(){
    tcx::pdsp::AudioStreamSettings as;as.sampleRate=48000;as.bufferSize=256;as.outputChannels=2;
    stream_.setup(as);
    synth_.prepare(stream_.context());
    synth_.setCutoff(3000);synth_.setResonance(0.3f);
    synth_.setAttack(0.02f);synth_.setDecay(0.15f);synth_.setSustain(0.5f);synth_.setRelease(0.3f);
    keyMap_['A']=60;keyMap_['W']=62;keyMap_['S']=64;keyMap_['E']=65;keyMap_['D']=67;
    keyMap_['F']=69;keyMap_['T']=71;keyMap_['G']=72;keyMap_['Y']=74;keyMap_['H']=76;
    keyMap_['U']=77;keyMap_['J']=79;keyMap_['K']=81;keyMap_['1']=60;keyMap_['2']=64;keyMap_['3']=67;
    stream_.setCallback([&](float*out,int frm,int ch){
        synth_.process(stream_.context(),frm);
        float*s=synth_.output().channel(0);
        for(int i=0;i<frm;i++){out[i*2]=s[i];out[i*2+1]=s[i];}
    });
    stream_.start();
}
void tcApp::update(){}
void tcApp::draw(){clear(0.12f);setColor(1.f);
    string s="Voices:"+to_string(activeNotes_.size())+" [A-K]notes [1-3]chords";
    drawBitmapString(s,12,16);}
void tcApp::keyPressed(int k){
    auto it=keyMap_.find(k);
    if(it!=keyMap_.end()){synth_.noteOn(it->second,1.0f);activeNotes_.push_back(it->second);}
    if(k=='1'){for(auto n:vector<int>{60,64,67})synth_.noteOn(n,1.0f);activeNotes_.push_back(60);}
    if(k=='2'){for(auto n:vector<int>{62,65,69})synth_.noteOn(n,1.0f);}
    if(k=='3'){for(auto n:vector<int>{67,71,74})synth_.noteOn(n,1.0f);}
}
void tcApp::keyReleased(int k){auto it=keyMap_.find(k);if(it!=keyMap_.end())synth_.noteOff(it->second);}
