// tcxPDSP Integration Verification — graph, transport, sequencer, analysis.
// Compile:
//   clang++ -std=c++17 -Isrc -o tests/pdsp_integration_test tests/pdsp_integration_test.cpp

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <vector>
#include <atomic>
#include <functional>
#include <array>

#include "core/Processor.h"
#include "dsp/SineOsc.h"
#include "dsp/Gain.h"
#include "sequencer/Transport.h"
#include "sequencer/StepSequencer.h"
#include "sequencer/EventQueue.h"
#include "analysis/FFTAnalyzer.h"
#include "analysis/OnsetDetector.h"
#include "analysis/PitchDetector.h"
#include "core/AudioBufferPool.h"
#include "core/ControlRate.h"
#include "dsp/SIMD.h"
#include "modules/DrumSynth.h"
#include "utils/OnePole.h"
#include "utils/SmoothRandom.h"

using namespace tcx::pdsp;

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { std::printf("  FAIL: %s\n", msg); failures++; } \
    else { std::printf("  PASS: %s\n", msg); } \
} while (0)

class CounterNode : public AudioNode {
public:
    explicit CounterNode(float value) : out_(this, 0), value_(value) {}
    void prepare(AudioContext& ctx) override {
        outputBuffer.allocate(1, ctx.bufferSize);
        prepared_ = true;
    }
    void process(AudioContext&, int frames) override {
        calls++;
        for (int i = 0; i < frames; i++) outputBuffer.sample(0, i) = value_;
    }
    PatchNode& out() { return out_; }
    int calls = 0;
private:
    PatchNode out_;
    float value_ = 0.0f;
};

static void test_lazy_graph() {
    std::printf("\n--- Lazy graph/topology ---\n");
    AudioContext ctx; ctx.sampleRate = 48000; ctx.bufferSize = 64; ctx.outputChannels = 2;

    Processor p;
    p.setup({48000, 64, 2});
    CounterNode connected(0.25f), unused(1.0f);
    connected.prepare(ctx);
    unused.prepare(ctx);

    connected.out() >> p.out(0);
    connected.out() >> p.out(1);
    p.addNode(&unused);
    p.addNode(&connected);
    p.rebuildGraph();

    float out[128] = {};
    p.process(out, 64, 2);
    CHECK(connected.calls == 1, "connected node processed");
    CHECK(unused.calls == 0, "unconnected node skipped");
    CHECK(std::fabs(out[0] - 0.25f) < 0.001f && std::fabs(out[1] - 0.25f) < 0.001f, "output is routed");
}

static void test_transport_loop() {
    std::printf("\n--- Transport loop ---\n");
    Transport t;
    t.prepare(48000);
    t.setBpm(120.0);
    t.setLoopSamples(100, 300);
    t.enableLoop(true);
    t.reset(250);
    t.play();
    t.advance(100);
    CHECK(t.currentSample() == 150, "advance wraps inside loop");
}

static void test_step_gate() {
    std::printf("\n--- StepSequencer gate/swing ---\n");
    AudioContext ctx; ctx.sampleRate = 48000; ctx.bufferSize = 512; ctx.outputChannels = 2;
    Transport t; t.prepare(48000); t.setBpm(120.0); t.play();
    StepSequencer seq; seq.prepare(48000); seq.setSteps(4); seq.setTimeSignature(4, 4);
    seq.setStep(0, true, 60, 1.0f);
    seq.setGateLength(0, 0.5f);
    EventQueue<32> q;

    seq.process(ctx, t, 512, q);
    SequenceEvent ev;
    CHECK(q.pop(ev) && ev.type == EventType::NoteOn && ev.sampleOffsetInBlock == 0, "first step emits note on");
    t.advance(512);

    bool gotOff = false;
    for (int block = 0; block < 64 && !gotOff; block++) {
        seq.process(ctx, t, 512, q);
        while (q.pop(ev)) {
            if (ev.type == EventType::NoteOff) gotOff = true;
        }
        t.advance(512);
    }
    CHECK(gotOff, "gate length emits note off");
}

static void test_analysis_and_utils() {
    std::printf("\n--- Analysis/utils ---\n");
    constexpr int sr = 48000;
    constexpr int n = 1024;
    float buffer[n];
    for (int i = 0; i < n; i++) {
        buffer[i] = std::sin(6.283185307f * 440.0f * i / sr);
    }

    PitchDetector pitch;
    pitch.prepare(sr, 80.0f, 1000.0f);
    float hz = pitch.process(buffer, n);
    CHECK(std::fabs(hz - 440.0f) < 10.0f, "pitch detector tracks 440Hz");

    FFTAnalyzer fft;
    fft.prepare(n);
    fft.process(buffer, n);
    CHECK(fft.peakBin() >= 8 && fft.peakBin() <= 11, "FFT peak near 440Hz bin");

    OnsetDetector onset;
    onset.prepare(0.05f);
    float silence[128] = {};
    CHECK(!onset.process(silence, 128), "silence has no onset");
    float hit[128];
    for (float& v : hit) v = 1.0f;
    CHECK(onset.process(hit, 128), "energy jump triggers onset");

    OnePole lp;
    lp.prepare(sr, 1000.0f);
    float y = 0.0f;
    for (int i = 0; i < 100; i++) y = lp.process(1.0f);
    CHECK(y > 0.5f && y <= 1.0f, "one-pole low-pass smooths toward target");

    SmoothRandom smooth;
    smooth.prepare(sr, 4.0f);
    float r = smooth.process();
    CHECK(r >= -1.0f && r <= 1.0f, "smooth random stays normalized");

    float dst[4] = {0, 0, 0, 0};
    float src[4] = {1, 2, 3, 4};
    simd::multiplyAdd(src, 0.5f, dst, 4);
    CHECK(dst[0] == 0.5f && dst[3] == 2.0f, "simd helpers process contiguous buffers");

    ControlRate control;
    control.prepare(sr, 1000.0f);
    int calls = 0;
    float heldA = control.process([&]() { calls++; return 0.25f; });
    float heldB = control.process([&]() { calls++; return 0.5f; });
    CHECK(calls == 1 && heldA == heldB, "control-rate value is held between updates");
}

static void test_pool_and_drumsynth() {
    std::printf("\n--- Buffer pool/DrumSynth ---\n");
    AudioBufferPool pool;
    pool.setup(2, 1, 64);
    AudioBuffer* a = pool.acquire();
    AudioBuffer* b = pool.acquire();
    AudioBuffer* c = pool.acquire();
    CHECK(a != nullptr && b != nullptr && c == nullptr, "fixed pool capacity is enforced");
    pool.release(a);
    CHECK(pool.acquire() == a, "released buffer can be reused");

    AudioContext ctx; ctx.sampleRate = 48000; ctx.bufferSize = 64; ctx.outputChannels = 2;
    DrumSynth drum;
    drum.prepare(ctx);
    drum.trigger(DrumSynth::Type::Kick, 1.0f);
    drum.process(ctx, 64);
    float peak = 0.0f;
    for (int i = 0; i < 64; i++) peak = std::max(peak, std::fabs(drum.output().sample(0, i)));
    CHECK(peak > 0.01f, "DrumSynth trigger produces audio");
}

int main() {
    std::printf("=== tcxPDSP Integration Verification ===\n");
    test_lazy_graph();
    test_transport_loop();
    test_step_gate();
    test_analysis_and_utils();
    test_pool_and_drumsynth();
    std::printf("\n=== %d failures ===\n", failures);
    return failures ? 1 : 0;
}
