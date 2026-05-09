#include "tcApp.h"
void tcApp::setup(){
    tcx::pdsp::AudioStreamSettings as; as.sampleRate=48000;as.bufferSize=256;as.outputChannels=2;
    stream_.setup(as);
    osc_.prepare(stream_.context());noise_.prepare(stream_.context());
    env_.prepare(48000);env_.setAttack(0.01f);env_.setRelease(0.2f);
    stream_.setCallback([&](float*out,int frm,int ch){
        auto&ctx=stream_.context();
        osc_.process(ctx,frm);if(noiseOn_)noise_.process(ctx,frm);
        float*o=osc_.output().channel(0),*n=noiseOn_?noise_.output().channel(0):nullptr;
        for(int i=0;i<frm;i++){float s=o[i]*0.15f;if(n)s+=n[i]*0.05f;out[i*2]=s;out[i*2+1]=s;}
        rms_.process(o,frm);peak_.process(o,frm);
        for(int i=0;i<frm;i++)env_.processSample(o[i]);
    });
    stream_.start();
}
void tcApp::update(){
    rmsVal_=rms_.value();peakVal_=peak_.value();envVal_=env_.value();
}
void tcApp::draw(){
    clear(0.05f);
    float cw=getWindowWidth(),ch=getWindowHeight();
    // Circle size driven by RMS
    float r=40+rmsVal_*400;
    setColor(0.3f+envVal_,0.5f,1.0f);fill();
    drawCircle(cw/3,ch/2,r);
    // Bar driven by peak
    setColor(1.0f,0.3f,0.2f);fill();
    drawRect(cw*2/3-20,ch-20,40,-peakVal_*ch*0.8f);
    // Envelope line
    setColor(1.0f);noFill();
    drawLine(cw/3,ch-30,cw/3+envVal_*cw/2,ch-30);
    setColor(0.8f);
    drawBitmapString("Sine+Noise | [N]toggle noise | RMS:"+to_string(rmsVal_).substr(0,4)+" Peak:"+to_string(peakVal_).substr(0,4),12,16);
}
void tcApp::keyPressed(int k){if(k=='N')noiseOn_=!noiseOn_;}
