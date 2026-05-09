#include "tcApp.h"
#include <algorithm>

void tcApp::setup() {
    tcx::pdsp::AudioStreamSettings audio;
    audio.sampleRate = 48000;
    audio.bufferSize = 256;
    audio.outputChannels = 2;
    stream_.setup(audio);

    auto& ctx = stream_.context();
    processor_.setup({stream_.sampleRate(), stream_.bufferSize(), stream_.outputChannels()});
    osc_.prepare(ctx);
    gain_.setGain(0.0f);
    gain_.prepare(ctx);
    delay_.prepare(ctx);
    delay_.setDelayTime(0.18f);
    delay_.setFeedback(0.25f);
    delay_.setWet(0.22f);

    osc_.out() >> gain_.in();
    gain_.out() >> delay_.in();
    delay_.out() >> processor_.out(0);
    delay_.out() >> processor_.out(1);
    processor_.addNode(&osc_);
    processor_.addNode(&gain_);
    processor_.addNode(&delay_);
    processor_.rebuildGraph();

    transport_.prepare(stream_.sampleRate());
    transport_.setBpm(118.0);
    transport_.setLoopBeats(0.0, 16.0);
    transport_.enableLoop(true);
    transport_.play();

    sequencer_.prepare(stream_.sampleRate());
    sequencer_.setSteps(12);
    sequencer_.setTimeSignature(3, 4);
    sequencer_.setSwing(0.22f);
    int notes[] = {60, 0, 67, 72, 0, 64, 69, 0, 74, 67, 0, 64};
    for (int i = 0; i < 12; i++) {
        sequencer_.setStep(i, notes[i] > 0, notes[i] > 0 ? notes[i] : 60, 0.75f);
        sequencer_.setGateLength(i, 0.42f);
    }

    env_.prepare(stream_.sampleRate());
    env_.setAttack(0.01f);
    env_.setRelease(0.18f);
    pitch_.prepare(stream_.sampleRate(), 80.0f, 1200.0f);
    onset_.prepare(0.04f);
    smoothLevel_.prepare(stream_.sampleRate(), 10.0f);

    stream_.setCallback([&](float* out, int frames, int channels) {
        auto& audioCtx = stream_.context();
        sequencer_.process(audioCtx, transport_, frames, events_);

        std::array<tcx::pdsp::SequenceEvent, 128> queued;
        int count = 0;
        tcx::pdsp::SequenceEvent event;
        while (count < static_cast<int>(queued.size()) && events_.pop(event)) {
            event.sampleOffsetInBlock = std::clamp(event.sampleOffsetInBlock, 0, frames);
            queued[count++] = event;
        }

        int cursor = 0;
        int eventIndex = 0;
        auto render = [&](int start, int length) {
            if (length <= 0) return;
            processor_.process(out + start * channels, length, channels);
            float* mono = delay_.output().channel(0);
            rms_.process(mono, length);
            peak_.process(mono, length);
            pitchVal_ = pitch_.process(mono, length);
            onsetVal_ = onset_.process(mono, length);
            for (int i = 0; i < length; i++) env_.processSample(mono[i]);
        };

        while (eventIndex < count) {
            int offset = queued[eventIndex].sampleOffsetInBlock;
            render(cursor, offset - cursor);
            cursor = offset;
            while (eventIndex < count && queued[eventIndex].sampleOffsetInBlock == offset) {
                applyEvent(queued[eventIndex++]);
            }
        }
        render(cursor, frames - cursor);
        transport_.advance(frames);
    });
    stream_.start();
}

void tcApp::applyEvent(const tcx::pdsp::SequenceEvent& event) {
    if (event.type == tcx::pdsp::EventType::NoteOn) {
        osc_.setFrequency(tcx::pdsp::math::midiToHz(event.note));
        gainTarget_ = event.value0 * 0.22f;
        gain_.setGain(gainTarget_);
    } else if (event.type == tcx::pdsp::EventType::NoteOff) {
        gainTarget_ = 0.0f;
        gain_.setGain(0.0f);
    }
}

void tcApp::update() {
    rmsVal_ = rms_.value();
    peakVal_ = peak_.value();
    envVal_ = env_.value();
}

void tcApp::draw() {
    clear(0.055f);
    float w = getWindowWidth();
    float h = getWindowHeight();

    setColor(0.16f, 0.18f, 0.21f);
    fill();
    drawRect(0, h * 0.55f, w, h * 0.45f);

    float radius = 32.0f + envVal_ * 180.0f;
    setColor(0.2f + envVal_, 0.72f, 0.55f);
    fill();
    drawCircle(w * 0.25f, h * 0.35f, radius);

    setColor(0.95f, 0.64f, 0.25f);
    fill();
    drawRect(w * 0.55f, h * 0.78f, 26.0f, -peakVal_ * h * 0.6f);
    setColor(0.38f, 0.62f, 1.0f);
    drawRect(w * 0.62f, h * 0.78f, 26.0f, -rmsVal_ * h * 0.9f);

    setColor(onsetVal_ ? 1.0f : 0.35f, onsetVal_ ? 0.25f : 0.55f, 0.35f);
    fill();
    drawCircle(w * 0.78f, h * 0.35f, onsetVal_ ? 44.0f : 22.0f);

    setColor(0.9f);
    drawBitmapString("3/4 sequencer + swing + loop | [Space] play | pitch "
        + to_string((int)pitchVal_) + "Hz", 14, 22);
    drawBitmapString("RMS " + to_string(rmsVal_).substr(0, 4)
        + " Peak " + to_string(peakVal_).substr(0, 4)
        + " Env " + to_string(envVal_).substr(0, 4), 14, 44);
}

void tcApp::keyPressed(int key) {
    if (key == KEY_SPACE) {
        playing_ = !playing_;
        if (playing_) transport_.play();
        else transport_.stop();
    }
}
