#pragma once
// tcxPDSP.h — TrussC Real-time DSP / Synthesis Addon

// Core
#include "core/AudioContext.h"
#include "core/AudioBuffer.h"
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/Processor.h"
#include "core/AudioBufferPool.h"
#include "core/ControlRate.h"
#include "core/Parameter.h"
#include "core/SmoothedValue.h"
#include "core/AudioStream.h"

// DSP
#include "dsp/SineOsc.h"
#include "dsp/Oscillator.h"
#include "dsp/Gain.h"
#include "dsp/Noise.h"
#include "dsp/ADSR.h"
#include "dsp/FilterSVF.h"
#include "dsp/Biquad.h"
#include "dsp/Delay.h"
#include "dsp/Mixer.h"
#include "dsp/Panner.h"
#include "dsp/Saturation.h"
#include "dsp/LFO.h"
#include "dsp/PulseOsc.h"
#include "dsp/Reverb.h"
#include "dsp/Chorus.h"
#include "dsp/Compressor.h"
#include "dsp/Effects.h"
#include "dsp/Effects2.h"
#include "dsp/SIMD.h"

// Modules
#include "modules/MonoSynth.h"
#include "modules/PolySynth.h"
#include "modules/DrumVoice.h"
#include "modules/DrumSynth.h"
#include "modules/SimpleSampler.h"
#include "modules/ChannelStrip.h"
#include "modules/Synths.h"
#include "modules/Advanced.h"

// Sequencer
#include "sequencer/Transport.h"
#include "sequencer/EventQueue.h"
#include "sequencer/StepSequencer.h"
#include "sequencer/Sequencer.h"
#include "sequencer/Pattern.h"
#include "sequencer/SequencerExt.h"

// Analysis
#include "analysis/RMS.h"
#include "analysis/PeakMeter.h"
#include "analysis/EnvelopeFollower.h"
#include "analysis/FFTAnalyzer.h"
#include "analysis/OnsetDetector.h"
#include "analysis/PitchDetector.h"

// Utils
#include "utils/Denormal.h"
#include "utils/Random.h"
#include "utils/SmoothRandom.h"
#include "utils/OnePole.h"
#include "utils/RingBuffer.h"
#include "utils/LockFreeQueue.h"
#include "utils/MathUtils.h"
