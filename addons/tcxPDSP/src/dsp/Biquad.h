#pragma once
// =============================================================================
// tcxPDSP Biquad — Biquadratic filter (LP/HP/BP/Notch/Peak/LowShelf/HighShelf)
// =============================================================================
// Direct Form I topology. Coefficients computed via RBJ Audio EQ Cookbook.

#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Parameter.h"
#include <cmath>

namespace tcx::pdsp {

class Biquad : public AudioNode {
public:
    enum class Type { LowPass, HighPass, BandPass, Notch, Peak, LowShelf, HighShelf };

    Biquad() : input_(this, 0), output_(this, 0) {
        outputBuffer.allocate(1, 256);
    }

    void prepare(AudioContext& ctx) override {
        sampleRate_ = static_cast<float>(ctx.sampleRate);
        outputBuffer.allocate(1, ctx.bufferSize);
        freq_.prepare(ctx.sampleRate, 10.0f);
        q_.prepare(ctx.sampleRate, 10.0f);
        gain_.prepare(ctx.sampleRate, 10.0f);
        x1_=x2_=y1_=y2_=0.0f;
        prepared_ = true;
        dirty_ = true;
    }

    void process(AudioContext& ctx, int frames) override {
        if (!prepared_ || !active_) return;
        ensureBuffer(frames);
        copyConnectedInput(input_, frames);
        float* out = outputBuffer.channel(0);

        for (int i = 0; i < frames; i++) {
            float f = freq_.next(), q = q_.next(), g = gain_.next();
            if (f != lastFreq_ || q != lastQ_ || g != lastGain_ || dirty_) {
                computeCoeffs(f, q, g);
                lastFreq_ = f; lastQ_ = q; lastGain_ = g; dirty_ = false;
            }
            float x = out[i];
            float y = b0_*x + b1_*x1_ + b2_*x2_ - a1_*y1_ - a2_*y2_;
            x2_=x1_; x1_=x;
            y2_=y1_; y1_=y;
            out[i] = y;
        }
    }

    PatchNode& in()  { return input_; }
    PatchNode& out() { return output_; }

    void setType(Type t)          { type_ = t; dirty_ = true; }
    void setFrequency(float hz)   { freq_.set(hz); }
    void setQ(float q)            { q_.set(q); }
    void setGain(float db)        { gain_.set(db); }

private:
    Type type_ = Type::LowPass;
    Parameter freq_{1000.0f}, q_{0.707f}, gain_{0.0f};
    PatchNode input_, output_;
    float sampleRate_ = 48000.0f;
    float b0_=0,b1_=0,b2_=0,a0_=0,a1_=0,a2_=0;
    float x1_=0,x2_=0,y1_=0,y2_=0;
    float lastFreq_=-1,lastQ_=-1,lastGain_=-1;
    bool dirty_ = false;

    void computeCoeffs(float freq, float Q, float gainDB) {
        float A  = std::pow(10.0f, gainDB / 40.0f);
        float w0 = 2.0f * 3.1415926535f * freq / sampleRate_;
        float cosW = std::cos(w0);
        float sinW = std::sin(w0);
        float alpha = sinW / (2.0f * Q);

        switch (type_) {
        case Type::LowPass:
            b0_ = (1.0f - cosW) * 0.5f;
            b1_ = 1.0f - cosW;
            b2_ = (1.0f - cosW) * 0.5f;
            a0_ = 1.0f + alpha;
            a1_ = -2.0f * cosW;
            a2_ = 1.0f - alpha;
            break;
        case Type::HighPass:
            b0_ = (1.0f + cosW) * 0.5f;
            b1_ = -(1.0f + cosW);
            b2_ = (1.0f + cosW) * 0.5f;
            a0_ = 1.0f + alpha;
            a1_ = -2.0f * cosW;
            a2_ = 1.0f - alpha;
            break;
        case Type::BandPass:
            b0_ = sinW * 0.5f;
            b1_ = 0.0f;
            b2_ = -sinW * 0.5f;
            a0_ = 1.0f + alpha;
            a1_ = -2.0f * cosW;
            a2_ = 1.0f - alpha;
            break;
        case Type::Notch:
            b0_ = 1.0f;
            b1_ = -2.0f * cosW;
            b2_ = 1.0f;
            a0_ = 1.0f + alpha;
            a1_ = -2.0f * cosW;
            a2_ = 1.0f - alpha;
            break;
        case Type::Peak:
            b0_ = 1.0f + alpha * A;
            b1_ = -2.0f * cosW;
            b2_ = 1.0f - alpha * A;
            a0_ = 1.0f + alpha / A;
            a1_ = -2.0f * cosW;
            a2_ = 1.0f - alpha / A;
            break;
        case Type::LowShelf:
            {
                float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
                b0_ = A * ((A+1.0f) - (A-1.0f)*cosW + twoSqrtAAlpha);
                b1_ = 2.0f * A * ((A-1.0f) - (A+1.0f)*cosW);
                b2_ = A * ((A+1.0f) - (A-1.0f)*cosW - twoSqrtAAlpha);
                a0_ = (A+1.0f) + (A-1.0f)*cosW + twoSqrtAAlpha;
                a1_ = -2.0f * ((A-1.0f) + (A+1.0f)*cosW);
                a2_ = (A+1.0f) + (A-1.0f)*cosW - twoSqrtAAlpha;
            }
            break;
        case Type::HighShelf:
            {
                float twoSqrtAAlpha = 2.0f * std::sqrt(A) * alpha;
                b0_ = A * ((A+1.0f) + (A-1.0f)*cosW + twoSqrtAAlpha);
                b1_ = -2.0f * A * ((A-1.0f) + (A+1.0f)*cosW);
                b2_ = A * ((A+1.0f) + (A-1.0f)*cosW - twoSqrtAAlpha);
                a0_ = (A+1.0f) - (A-1.0f)*cosW + twoSqrtAAlpha;
                a1_ = 2.0f * ((A-1.0f) - (A+1.0f)*cosW);
                a2_ = (A+1.0f) - (A-1.0f)*cosW - twoSqrtAAlpha;
            }
            break;
        }
        // Normalize
        float invA0 = 1.0f / a0_;
        b0_ *= invA0; b1_ *= invA0; b2_ *= invA0;
        a1_ *= invA0; a2_ *= invA0;
    }

    void ensureBuffer(int frames) {
        if (outputBuffer.frames() < frames) outputBuffer.allocate(1, frames);
    }
};

} // namespace tcx::pdsp
