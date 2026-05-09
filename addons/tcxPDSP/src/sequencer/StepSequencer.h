#pragma once
// =============================================================================
// tcxPDSP StepSequencer — 16-step pattern sequencer (sample-accurate)
// =============================================================================

#include "core/AudioContext.h"
#include "sequencer/Transport.h"
#include "sequencer/EventQueue.h"
#include <vector>
#include <cstring>
#include <algorithm>

namespace tcx::pdsp {

class StepSequencer {
public:
    struct Step {
        bool  active      = false;
        int   note        = 60;
        float velocity    = 1.0f;
        float probability = 1.0f;
        float gateLength  = 0.8f;  // fraction of step duration
    };

    void prepare(int sampleRate) {
        sampleRate_ = sampleRate;
        stepsPerBar_ = 16;
        steps_.resize(stepsPerBar_);
        currentStep_ = -1;  // -1 ensures step 0 fires on first beat
        pendingNoteOff_ = false;
    }

    void setSteps(int count) {
        stepsPerBar_ = count;
        steps_.resize(count);
    }

    void setDivision(int div) {
        stepsPerBar_ = div;
        steps_.resize(div);
    }

    void setTimeSignature(int beatsPerBar, int beatUnit = 4) {
        beatsPerBar_ = std::max(1, beatsPerBar);
        beatUnit_ = std::max(1, beatUnit);
    }

    void setStep(int index, bool active, int note, float velocity, float probability = 1.0f) {
        if (index < 0 || index >= (int)steps_.size()) return;
        steps_[index].active = active;
        steps_[index].note = note;
        steps_[index].velocity = velocity;
        steps_[index].probability = probability;
    }

    void setGateLength(int index, float gateLen) {
        if (index < 0 || index >= (int)steps_.size()) return;
        steps_[index].gateLength = gateLen;
    }

    void setSwing(float amount) {
        swing_ = std::clamp(amount, 0.0f, 1.0f);
    }

    // Process audio block, push events to queue
    template<int QSize>
    void process(AudioContext& ctx, Transport& transport, int frames, EventQueue<QSize>& queue) {
        if (!transport.isPlaying()) return;

        uint64_t spb = transport.samplesPerBeat();
        uint64_t samplesPerBar = static_cast<uint64_t>(
            static_cast<double>(spb) * static_cast<double>(beatsPerBar_) * (4.0 / static_cast<double>(beatUnit_)));
        uint64_t samplesPerStep = samplesPerBar / static_cast<uint64_t>(stepsPerBar_);

        if (samplesPerStep == 0) return;

        for (int f = 0; f < frames; f++) {
            uint64_t absSample = transport.currentSample() + f;
            uint64_t sequenceStep = absSample / samplesPerStep;
            uint64_t stepIndex = sequenceStep % stepsPerBar_;
            uint64_t swingOffset = (stepIndex % 2 == 1)
                ? static_cast<uint64_t>((samplesPerStep / 2) * swing_)
                : 0;
            if (swingOffset >= samplesPerStep) swingOffset = samplesPerStep - 1;
            uint64_t triggerSample = sequenceStep * samplesPerStep + swingOffset;

            if (pendingNoteOff_ && absSample >= pendingNoteOffSample_) {
                SequenceEvent offEv;
                offEv.type = EventType::NoteOff;
                offEv.sampleTime = absSample;
                offEv.sampleOffsetInBlock = f;
                offEv.note = pendingNote_;
                queue.push(offEv);
                pendingNoteOff_ = false;
            }

            int step = static_cast<int>(stepIndex);
            if (step != currentStep_ && absSample == triggerSample) {
                currentStep_ = step;

                if (steps_[step].active) {
                    float r = nextRandom();
                    if (r < steps_[step].probability) {
                        // NoteOn for this step
                        SequenceEvent ev;
                        ev.type = EventType::NoteOn;
                        ev.sampleTime = absSample;
                        ev.sampleOffsetInBlock = f;
                        ev.note = steps_[step].note;
                        ev.value0 = steps_[step].velocity;
                        queue.push(ev);

                        float gate = std::clamp(steps_[step].gateLength, 0.0f, 1.0f);
                        uint64_t gateSamples = std::max<uint64_t>(1, static_cast<uint64_t>(samplesPerStep * gate));
                        pendingNote_ = steps_[step].note;
                        pendingNoteOffSample_ = absSample + gateSamples;
                        pendingNoteOff_ = true;
                    }
                } else {
                    // Rest step: send note-off to silence previous note
                    SequenceEvent offEv;
                    offEv.type = EventType::NoteOff;
                    offEv.sampleTime = absSample;
                    offEv.sampleOffsetInBlock = f;
                    offEv.note = 0;
                    queue.push(offEv);
                }
            }
        }
    }

    int getCurrentStep() const { return currentStep_; }
    const std::vector<Step>& getSteps() const { return steps_; }

private:
    std::vector<Step> steps_;
    int sampleRate_ = 48000;
    int stepsPerBar_ = 16;
    int beatsPerBar_ = 4;
    int beatUnit_ = 4;
    int currentStep_ = -1;
    float swing_ = 0.0f;
    bool pendingNoteOff_ = false;
    int pendingNote_ = 60;
    uint64_t pendingNoteOffSample_ = 0;
    uint32_t seed_ = 0x12345678u;

    float nextRandom() {
        seed_ ^= seed_ << 13;
        seed_ ^= seed_ >> 17;
        seed_ ^= seed_ << 5;
        return static_cast<float>(seed_ & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
    }
};

} // namespace tcx::pdsp
