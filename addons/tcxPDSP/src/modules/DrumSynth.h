#pragma once
// DrumSynth — pitch envelope + body/noise/click drum voice.

#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "utils/Random.h"
#include <cmath>
#include <algorithm>

namespace tcx::pdsp {

class DrumSynth : public AudioNode {
public:
    enum class Type { Kick, Snare, Hat };

    DrumSynth() : out_(this, 0) {
        outputBuffer.allocate(1, 256);
    }

    void prepare(AudioContext& ctx) override {
        sampleRate_ = static_cast<float>(ctx.sampleRate);
        outputBuffer.allocate(1, ctx.bufferSize);
        prepared_ = true;
    }

    void trigger(Type type, float velocity = 1.0f) {
        type_ = type;
        velocity_ = std::clamp(velocity, 0.0f, 1.0f);
        age_ = 0.0f;
        phase_ = 0.0f;
        activeVoice_ = true;
    }

    void process(AudioContext&, int frames) override {
        if (!prepared_ || !active_) return;
        ensureBuffer(frames);
        float* out = outputBuffer.channel(0);
        for (int i = 0; i < frames; i++) {
            out[i] = activeVoice_ ? processSample() * velocity_ : 0.0f;
            age_ += 1.0f / sampleRate_;
            if (age_ > maxDuration()) activeVoice_ = false;
        }
    }

    PatchNode& out() { return out_; }

private:
    float processSample() {
        switch (type_) {
            case Type::Kick: {
                float env = std::exp(-age_ * 10.5f);
                float pitchEnv = std::exp(-age_ * 28.0f);
                float freq = 45.0f + 95.0f * pitchEnv;
                phase_ += freq / sampleRate_;
                if (phase_ >= 1.0f) phase_ -= 1.0f;
                float body = std::sin(phase_ * 6.283185307f) * env;
                float click = age_ < 0.004f ? (1.0f - age_ / 0.004f) * 0.35f : 0.0f;
                return body + click;
            }
            case Type::Snare: {
                float bodyEnv = std::exp(-age_ * 10.0f);
                float noiseEnv = std::exp(-age_ * 16.0f);
                phase_ += 190.0f / sampleRate_;
                if (phase_ >= 1.0f) phase_ -= 1.0f;
                float body = std::sin(phase_ * 6.283185307f) * bodyEnv * 0.45f;
                float noise = (rng_.next() * 2.0f - 1.0f) * noiseEnv * 0.65f;
                return body + noise;
            }
            case Type::Hat: {
                float env = std::exp(-age_ * 42.0f);
                float noise = rng_.next() * 2.0f - 1.0f;
                lastHp_ = noise - lastNoise_ + 0.82f * lastHp_;
                lastNoise_ = noise;
                return lastHp_ * env * 0.5f;
            }
        }
        return 0.0f;
    }

    float maxDuration() const {
        switch (type_) {
            case Type::Kick: return 0.7f;
            case Type::Snare: return 0.55f;
            case Type::Hat: return 0.25f;
        }
        return 0.5f;
    }

    void ensureBuffer(int frames) {
        if (outputBuffer.frames() < frames) outputBuffer.allocate(1, frames);
    }

    PatchNode out_;
    Type type_ = Type::Kick;
    float sampleRate_ = 48000.0f;
    float phase_ = 0.0f;
    float age_ = 0.0f;
    float velocity_ = 1.0f;
    float lastNoise_ = 0.0f;
    float lastHp_ = 0.0f;
    bool activeVoice_ = false;
    Random rng_;
};

} // namespace tcx::pdsp
