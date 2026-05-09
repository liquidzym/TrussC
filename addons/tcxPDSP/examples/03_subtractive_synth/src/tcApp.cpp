#include "tcApp.h"
void tcApp::setup(){
    tcx::pdsp::AudioStreamSettings as; as.sampleRate=48000;as.bufferSize=256;as.outputChannels=2;
    stream_.setup(as);
    synth_.prepare(stream_.context());
    synth_.setCutoff(2000);synth_.setResonance(0.5f);
    synth_.setAttack(0.02f);synth_.setDecay(0.1f);synth_.setSustain(0.6f);synth_.setRelease(0.2f);

    keyMap_['A']=60;keyMap_['W']=61;keyMap_['S']=62;keyMap_['E']=63;keyMap_['D']=64;
    keyMap_['F']=65;keyMap_['T']=66;keyMap_['G']=67;keyMap_['Y']=68;keyMap_['H']=69;
    keyMap_['U']=70;keyMap_['J']=71;keyMap_['K']=72;

    stream_.setCallback([&](float*out,int frm,int ch){
        auto&ctx=stream_.context();
        synth_.process(ctx,frm);
        float*s=synth_.output().channel(0);
        for(int i=0;i<frm;i++){float v=s[i];out[i*2]=v;out[i*2+1]=v;}
        rms_.process(s,frm);
    });
    stream_.start();
}
void tcApp::update(){float v=rms_.value();if(v>peak_)peak_=v;peak_*=0.99f;}
void tcApp::draw(){clear(0.12f);setColor(1.f);
    drawBitmapString("MonoSynth: [A-K] keys | Cutoff:"+to_string((int)peak_*1000),12,16);}
void tcApp::keyPressed(int k){
    auto it=keyMap_.find(k);if(it!=keyMap_.end())synth_.noteOn(it->second,1.0f);
    if(k==KEY_UP)synth_.setCutoff(cutoff_+=200);
    if(k==KEY_DOWN)synth_.setCutoff(cutoff_-=200);
}
void tcApp::keyReleased(int k){auto it=keyMap_.find(k);if(it!=keyMap_.end())synth_.noteOff();}
