#include "tcApp.h"

void tcApp::setup() {
    tcx::pdsp::AudioStreamSettings as;
    as.sampleRate = 48000;
    as.bufferSize = 256;
    as.outputChannels = 2;
    stream_.setup(as);

    osc_.prepare(stream_.context());
    gain_.prepare(stream_.context());
    osc_.setFrequency(freq_);
    gain_.setGain(0.15f);

    stream_.setCallback([&](float* out, int frames, int ch) {
        // Run osc → gain pipeline
        auto& ctx = stream_.context();
        osc_.process(ctx, frames);
        float* oscBuf = osc_.output().channel(0);
        for (int i = 0; i < frames; i++) {
            float s = oscBuf[i] * 0.15f;
            out[i*2]   = s;
            out[i*2+1] = s;
        }
    });

    stream_.start();
}

void tcApp::update() {}
void tcApp::draw() {
    clear(0.12f);
    setColor(1.0f);
    drawBitmapString("Sine: " + to_string((int)freq_) + " Hz | [Up/Down] freq | [Q] quit", 12, 16);
}

void tcApp::keyPressed(int key) {
    if (key == KEY_UP)   { freq_ += 10; osc_.setFrequency(freq_); }
    if (key == KEY_DOWN) { freq_ -= 10; if (freq_ < 20) freq_ = 20; osc_.setFrequency(freq_); }
}
