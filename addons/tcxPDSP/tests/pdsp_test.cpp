// =============================================================================
// tcxPDSP Verification Test — No audio device needed
// Compile: clang++ -std=c++17 -I../src -o pdsp_test pdsp_test.cpp && ./pdsp_test
// =============================================================================

#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>

// Minimal Vec3 (the DSP headers don't need graphics Vec3)
// Actually, we need to handle the TrussC include issue. Let's avoid TrussC entirely.
// The DSP headers only need: <cstdint>, <cmath>, <vector>, <atomic>, <functional>

// Since the headers use tcx::pdsp namespace, we just include them directly.
// They ultimately include <TrussC.h> through AudioContext.h which uses uint64_t.
// But uint64_t is from <cstdint> which we have.

// Let's just define the absolute minimum to compile:
#include <cstdint>
#include <atomic>
#include <vector>
#include <functional>
#include <algorithm>
#include <array>

// Include tcxPDSP headers (they use TrussC only for uint64_t which we already have from cstdint)
#include "core/AudioBuffer.h"
#include "core/SmoothedValue.h"
#include "core/Parameter.h"
#include "core/PatchNode.h"
#include "core/Processor.h"
#include "dsp/SineOsc.h"
#include "dsp/Gain.h"
#include "dsp/Mixer.h"
#include "dsp/ADSR.h"
#include "analysis/RMS.h"
#include "analysis/PeakMeter.h"
#include "analysis/EnvelopeFollower.h"
#include "sequencer/Transport.h"
#include "sequencer/EventQueue.h"
#include "utils/LockFreeQueue.h"
#include "utils/MathUtils.h"

using namespace tcx::pdsp;

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
    else { printf("  PASS: %s\n", msg); } \
} while(0)

#define CHECK_NEAR(a, b, eps, msg) do { \
    float _a=(float)(a),_b=(float)(b); \
    if (fabsf(_a-_b) > (eps)) { \
        printf("  FAIL: %s (got %.4f expected %.4f)\n", msg, _a, _b); failures++; \
    } else { printf("  PASS: %s\n", msg); } \
} while(0)

// =============================================================================
// Test 1: AudioBuffer
// =============================================================================
void test_audio_buffer() {
    printf("\n--- Test 1: AudioBuffer ---\n");
    AudioBuffer buf;
    buf.allocate(2, 128);
    CHECK(buf.channels() == 2, "2 channels");
    CHECK(buf.frames() == 128, "128 frames");
    CHECK(!buf.empty(), "not empty");

    buf.sample(0, 10) = 0.5f;
    buf.sample(1, 20) = -0.3f;
    CHECK_NEAR(buf.sample(0, 10), 0.5f, 0.001f, "sample(0,10)=0.5");
    CHECK_NEAR(buf.sample(1, 20), -0.3f, 0.001f, "sample(1,20)=-0.3");

    buf.clear();
    CHECK_NEAR(buf.sample(0, 10), 0.0f, 0.001f, "cleared to zero");
}

// =============================================================================
// Test 2: SmoothedValue
// =============================================================================
void test_smoothed() {
    printf("\n--- Test 2: SmoothedValue ---\n");
    SmoothedValue sv(0.0f);
    sv.setTarget(1.0f);
    sv.setTime(10.0f, 1000); // 10ms at 1000Hz = 10 samples

    float val = 0.0f;
    for (int i = 0; i < 15; i++) val = sv.next();
    CHECK_NEAR(val, 1.0f, 0.001f, "Reaches target after ramp");

    sv.setTarget(0.5f);
    sv.setTime(0.0f, 1000); // instant
    val = sv.next();
    CHECK_NEAR(val, 0.5f, 0.001f, "Instant jump");
}

// =============================================================================
// Test 3: Parameter
// =============================================================================
void test_parameter() {
    printf("\n--- Test 3: Parameter (thread-safe) ---\n");
    Parameter p(0.5f);
    CHECK_NEAR(p.getTarget(), 0.5f, 0.001f, "Initial value 0.5");

    p.set(0.8f);
    CHECK_NEAR(p.getTarget(), 0.8f, 0.001f, "set(0.8) updates target");

    p.prepare(1000, 5.0f);  // 5ms smoothing at 1kHz = 5 samples
    float v = 0.0f;
    for (int i = 0; i < 20; i++) v = p.next();
    CHECK_NEAR(v, 0.8f, 0.001f, "Reaches target");

    p.set(0.2f);
    for (int i = 0; i < 20; i++) v = p.next();
    CHECK_NEAR(v, 0.2f, 0.01f, "Reaches new target");
}

// =============================================================================
// Test 4: ADSR Envelope
// =============================================================================
void test_adsr() {
    printf("\n--- Test 4: ADSR Envelope ---\n");
    ADSR env;
    env.setSampleRate(1000.0f);
    env.setAttack(0.01f);   // 10 samples
    env.setDecay(0.02f);    // 20 samples
    env.setSustain(0.5f);
    env.setRelease(0.03f);  // 30 samples

    CHECK(!env.isActive(), "Starts idle");

    env.noteOn();
    CHECK(env.isActive(), "Active after noteOn");

    // Process through attack (10 samples at 1kHz)
    float v = 0.0f;
    float peak = 0.0f;
    for (int i = 0; i < 10; i++) { v = env.process(); if (v > peak) peak = v; }
    CHECK(peak > 0.99f, "Reaches attack peak (within attack phase)");

    // Process through decay
    for (int i = 0; i < 30; i++) v = env.process();
    CHECK_NEAR(v, 0.5f, 0.05f, "Reaches sustain");

    env.noteOff();
    for (int i = 0; i < 50; i++) v = env.process();
    CHECK_NEAR(v, 0.0f, 0.01f, "Reaches zero after release");
    CHECK(!env.isActive(), "Inactive after release");
}

// =============================================================================
// Test 5: PeakMeter
// =============================================================================
void test_peak_meter() {
    printf("\n--- Test 5: PeakMeter ---\n");
    PeakMeter meter;
    float buf[4] = {0.1f, -0.5f, 0.3f, -0.2f};
    meter.process(buf, 4);
    CHECK_NEAR(meter.value(), 0.5f, 0.001f, "Peak = 0.5");
}

// =============================================================================
// Test 6: RMS
// =============================================================================
void test_rms() {
    printf("\n--- Test 6: RMS ---\n");
    RMS rms;
    float buf[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    rms.process(buf, 4);
    CHECK_NEAR(rms.value(), 0.5f, 0.001f, "RMS of constant 0.5");
}

// =============================================================================
// Test 7: EnvelopeFollower
// =============================================================================
void test_env_follower() {
    printf("\n--- Test 7: EnvelopeFollower ---\n");
    EnvelopeFollower ef;
    ef.prepare(1000);
    ef.setAttack(0.01f);
    ef.setRelease(0.1f);

    float v = 0.0f;
    for (int i = 0; i < 100; i++) v = ef.processSample(1.0f);
    CHECK(v > 0.5f, "Rises toward 1.0");

    for (int i = 0; i < 200; i++) v = ef.processSample(0.0f);
    CHECK(v < 0.5f, "Falls toward 0.0");
}

// =============================================================================
// Test 8: EventQueue
// =============================================================================
void test_event_queue() {
    printf("\n--- Test 8: EventQueue ---\n");
    EventQueue<8> q;
    CHECK(q.empty(), "Starts empty");

    SequenceEvent e;
    e.type = EventType::NoteOn;
    e.note = 60;
    CHECK(q.push(e), "Push succeeds");
    CHECK(!q.empty(), "Not empty after push");

    SequenceEvent out;
    CHECK(q.pop(out), "Pop succeeds");
    CHECK(out.note == 60, "Note preserved");
    CHECK(out.type == EventType::NoteOn, "Type preserved");
    CHECK(q.empty(), "Empty after pop");
}

// =============================================================================
// Test 9: Transport
// =============================================================================
void test_transport() {
    printf("\n--- Test 9: Transport ---\n");
    Transport t;
    t.prepare(48000);
    t.setBpm(120.0);
    CHECK(!t.isPlaying(), "Starts stopped");

    t.play();
    CHECK(t.isPlaying(), "Playing after play()");

    uint64_t spb = t.samplesPerBeat();
    // 120 BPM = 0.5 sec/beat = 24000 samples
    CHECK_NEAR((double)spb, 24000.0, 1.0, "samplesPerBeat at 120 BPM");

    t.advance(100);
    CHECK(t.currentSample() == 100, "Advanced 100 samples");

    t.stop();
    t.advance(100);
    CHECK(t.currentSample() == 100, "No advance when stopped");
}

// =============================================================================
// Test 10: PatchNode connections
// =============================================================================
void test_patch_node() {
    printf("\n--- Test 10: PatchNode ---\n");
    PatchNode src(nullptr, 0);
    PatchNode dst(nullptr, 1);

    src >> dst;
    CHECK(src.destinations().size() == 1, "Connection established");
    CHECK(src.destinations()[0] == &dst, "Destination correct");

    src.disconnect(dst);
    CHECK(src.destinations().size() == 0, "Disconnected");
}

// =============================================================================
// Test 11: ADSR retrigger
// =============================================================================
void test_adsr_retrigger() {
    printf("\n--- Test 11: ADSR Retrigger ---\n");
    ADSR env;
    env.setSampleRate(1000.0f);
    env.setAttack(0.01f);
    env.setDecay(0.1f);
    env.setSustain(0.5f);
    env.setRelease(0.1f);

    env.noteOn();
    for (int i = 0; i < 5; i++) env.process(); // mid-attack (~0.5)
    float mid = env.process();
    env.noteOn(); // retrigger during attack is ignored (protects against clicks)
    float after = env.process();
    CHECK(after > mid, "Retrigger during attack ignored, keeps rising");
}

// =============================================================================
// Test 12: Mixer pan law
// =============================================================================
void test_mixer() {
    printf("\n--- Test 12: Mixer ---\n");
    Mixer mixer;
    int ch0 = mixer.addInput();
    int ch1 = mixer.addInput();
    CHECK(mixer.numChannels() == 2, "2 channels added");

    // Set levels
    mixer.setLevel(0, 0.5f);
    mixer.setLevel(1, 0.3f);
    mixer.setPan(0, -1.0f);  // full left
    mixer.setPan(1, 1.0f);   // full right

    // Fill input buffers
    AudioContext ctx;
    ctx.sampleRate = 48000;
    ctx.bufferSize = 64;
    mixer.prepare(ctx);

    auto& b0 = mixer.inputBuffer(0);
    auto& b1 = mixer.inputBuffer(1);
    for (int i = 0; i < 64; i++) { b0.sample(0, i) = 1.0f; b1.sample(0, i) = 0.5f; }

    mixer.process(ctx, 64);

    // Channel 0 (left): level=0.5, pan=-1 → left output should be non-zero
    CHECK(mixer.output().sample(0, 0) > 0.3f, "Left channel has signal (panned left)");
    // Channel 1 (right): level=0.3, pan=1 → right output should be non-zero  
    CHECK(mixer.output().sample(1, 0) > 0.1f, "Right channel has signal (panned right)");
    // Left should be mostly from ch0, right from ch1
    // Left: ch0 (pan=-1) → only left, Right: ch1 (pan=1) → only right
    float lv = mixer.output().sample(0, 0);
    float rv = mixer.output().sample(1, 0);
    printf("  mixer L=%.4f R=%.4f\n", lv, rv);
    CHECK(lv > 0.35f && rv > 0.05f, "Both channels have signal");
    CHECK(lv != rv, "Left and right are different (panned)");
}

// =============================================================================
// Test 13: Processor patch graph
// =============================================================================
void test_processor_graph() {
    printf("\n--- Test 13: Processor Patch Graph ---\n");
    AudioContext ctx; ctx.sampleRate = 48000; ctx.bufferSize = 64; ctx.outputChannels = 2;
    Processor p;
    p.setup({48000, 64, 2});

    SineOsc osc;
    Gain gain;
    gain.setGain(0.25f);
    osc.prepare(ctx);
    gain.prepare(ctx);
    osc.setFrequency(440.0f);

    osc.out() >> gain.in();
    gain.out() >> p.out(0);
    gain.out() >> p.out(1);
    p.addNode(&osc);
    p.addNode(&gain);

    float out[64 * 2] = {};
    p.process(out, 64, 2);

    float peak = 0.0f;
    for (int i = 0; i < 64 * 2; i++) peak = std::max(peak, std::fabs(out[i]));
    CHECK(peak > 0.01f, "Connected graph produces output");
    CHECK(peak < 0.35f, "Gain node limits output level");
}

// =============================================================================
// Test 14: MathUtils
// =============================================================================
void test_math_utils() {
    printf("\n--- Test 14: MathUtils ---\n");
    using namespace tcx::pdsp::math;
    CHECK_NEAR(midiToHz(69), 440.0f, 0.01f, "A4 = 440Hz");
    CHECK_NEAR(midiToHz(60), 261.6256f, 0.1f, "C4 ≈ 261.6Hz");
    CHECK_NEAR(hzToMidi(440.0f), 69.0f, 0.01f, "440Hz → MIDI 69");
    CHECK_NEAR(dbToGain(0.0f), 1.0f, 0.001f, "0dB = gain 1.0");
    CHECK_NEAR(dbToGain(-6.0f), 0.5f, 0.01f, "-6dB ≈ gain 0.5");
    CHECK_NEAR(semitoneToRatio(12.0f), 2.0f, 0.001f, "12 semitones = 2x");
    CHECK_NEAR(bpmToBeatDuration(120.0f), 0.5f, 0.001f, "120BPM beat = 0.5s");
    CHECK_NEAR(map(0.5f, 0, 1, 0, 100), 50.0f, 0.01f, "map 0.5→50");
    CHECK_NEAR(clamp(1.5f, 0, 1), 1.0f, 0.001f, "clamp 1.5→1.0");
    CHECK_NEAR(lerp(0, 10, 0.3f), 3.0f, 0.001f, "lerp 0→10 at 0.3");
}

// =============================================================================
// Test 15: LockFreeQueue
// =============================================================================
void test_lockfree_queue() {
    printf("\n--- Test 15: LockFreeQueue ---\n");
    LockFreeQueue<int, 8> q;
    CHECK(q.empty(), "Starts empty");
    CHECK(q.size() == 0, "Size 0");
    CHECK(q.push(42), "Push succeeds");
    CHECK(!q.empty(), "Not empty");
    int v;
    CHECK(q.pop(v) && v == 42, "Pop returns 42");
    CHECK(q.empty(), "Empty after pop");
    // Fill to capacity-1 (one slot reserved)
    for(int i=0;i<7;i++) q.push(i*10);
    CHECK(!q.push(99), "Push fails when full (7 items = capacity-1)");
    CHECK(q.size() == 7, "Size is 7");
    q.clear();
    CHECK(q.empty(), "Empty after clear");
}

// =============================================================================
// main
// =============================================================================
int main() {
    printf("=== tcxPDSP Verification Suite ===\n");

    test_audio_buffer();
    test_smoothed();
    test_parameter();
    test_adsr();
    test_peak_meter();
    test_rms();
    test_env_follower();
    test_event_queue();
    test_transport();
    test_patch_node();
    test_adsr_retrigger();
    test_mixer();
    test_processor_graph();
    test_math_utils();
    test_lockfree_queue();

    printf("\n=== %d failures ===\n", failures);
    return failures ? 1 : 0;
}
