#pragma once
// =============================================================================
// tcxPDSP MathUtils — Audio/music math helpers
// =============================================================================

#include <cmath>
#include <algorithm>
#include <cstdint>

namespace tcx::pdsp {

namespace math {

// MIDI note to frequency (A4=440Hz, MIDI 69)
inline float midiToHz(int midiNote) {
    return 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
}

// Frequency to MIDI note (returns float for pitch bend)
inline float hzToMidi(float hz) {
    if (hz <= 0.0f) return 0.0f;
    return 69.0f + 12.0f * std::log2(hz / 440.0f);
}

// Linear map: value from [inMin, inMax] → [outMin, outMax]
inline float map(float value, float inMin, float inMax, float outMin, float outMax) {
    if (inMax == inMin) return outMin;
    return outMin + (outMax - outMin) * (value - inMin) / (inMax - inMin);
}

// Clamp value to [min, max]
inline float clamp(float value, float minVal, float maxVal) {
    return std::max(minVal, std::min(value, maxVal));
}

// Linear interpolation
inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

// Convert dB to linear gain
inline float dbToGain(float db) {
    return std::pow(10.0f, db / 20.0f);
}

// Convert linear gain to dB
inline float gainToDb(float gain) {
    if (gain <= 0.0f) return -96.0f;
    return 20.0f * std::log10(gain);
}

// Semitone to frequency ratio (equal temperament)
inline float semitoneToRatio(float semitones) {
    return std::pow(2.0f, semitones / 12.0f);
}

// BPM to beat duration in seconds
inline float bpmToBeatDuration(float bpm) {
    if (bpm <= 0.0f) return 1.0f;
    return 60.0f / bpm;
}

// BPM to samples per beat
inline uint64_t bpmToSamplesPerBeat(float bpm, int sampleRate) {
    if (bpm <= 0.0f) return static_cast<uint64_t>(sampleRate);
    return static_cast<uint64_t>(60.0 * sampleRate / bpm);
}

// Check if float is denormal (subnormal)
inline bool isDenormal(float x) {
    return x != 0.0f && std::fabs(x) < 1.17549435e-38f;
}

} // namespace math

} // namespace tcx::pdsp
