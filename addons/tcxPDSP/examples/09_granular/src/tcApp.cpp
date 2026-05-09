#include "tcApp.h"
void tcApp::setup(){
    tcx::pdsp::AudioStreamSettings as;as.sampleRate=48000;as.bufferSize=256;as.outputChannels=2;
    stream_.setup(as);gran_.prepare(stream_.context());pm_.prepare(stream_.context());
    // Generate a simple sine sample for granular
    vector<float> sample(48000);for(int i=0;i<48000;i++)sample[i]=sinf(i/48000.0f*440*6.2831853f)*0.5f;
    gran_.loadSample(sample.data(),48000);
    pm_.setFrequency(220);pm_.setDecay(0.9999f);pm_.pluck(0.5f);
    stream_.setCallback([&](float*out,int frm,int ch){
        auto&ctx=stream_.context();
        if(mode_==0){gran_.process(ctx,frm);float*s=gran_.output().channel(0);for(int i=0;i<frm;i++)out[i*2]=out[i*2+1]=s[i];}
        else{pm_.process(ctx,frm);float*s=pm_.output().channel(0);for(int i=0;i<frm;i++)out[i*2]=out[i*2+1]=s[i]*0.3f;}
    });
    stream_.start();
}
void tcApp::update(){}
void tcApp::draw(){clear(0.1f);setColor(1.f);
    string info=mode_==0?"Granular | Dens:"+to_string((int)density_)+" Size:"+to_string(grainSize_).substr(0,3)+" Pitch:"+to_string(pitch_).substr(0,3)
                       :"PhysicalModel (Karplus-Strong) | [Space]pluck [Up/Down]freq";
    drawBitmapString(info+" | [1/2]mode [Q/W/A/S/Z/X]params",12,16);}
void tcApp::keyPressed(int k){
    if(k=='1')mode_=0;
    if(k=='2'){pm_.pluck(0.5f);mode_=1;}
    if(mode_==0){
        if(k=='Q'){density_+=2;gran_.setDensity(density_);}if(k=='W'){density_-=2;if(density_<1)density_=1;gran_.setDensity(density_);}
        if(k=='A'){grainSize_+=0.05f;gran_.setGrainSize(grainSize_);}if(k=='S'){grainSize_-=0.05f;if(grainSize_<0.02f)grainSize_=0.02f;gran_.setGrainSize(grainSize_);}
        if(k=='Z'){pitch_+=0.1f;gran_.setPitch(pitch_);}if(k=='X'){pitch_-=0.1f;if(pitch_<0.1f)pitch_=0.1f;gran_.setPitch(pitch_);}
    }else{
        if(k==KEY_SPACE)pm_.pluck(0.5f);
        if(k==KEY_UP){pmFreq_+=20;pm_.setFrequency(pmFreq_);}
        if(k==KEY_DOWN){pmFreq_-=20;if(pmFreq_<20)pmFreq_=20;pm_.setFrequency(pmFreq_);}
    }
}
