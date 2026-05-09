# `tcxPDSP`：TrussC 原生 DSP / 生成音乐 addon 实现任务书

## 0. 总目标

为 TrussC 编写一个原生 addon：

```text
addons/tcxPDSP/
```

它参考 `ofxPDSP` 的核心设计，但不是直接移植 openFrameworks 外壳。

目标是给 TrussC 增加一套：

```text
实时 DSP graph
模块化 patch 系统
oscillator / filter / envelope / delay / mixer
sample-accurate sequencer
synth / drum / sampler 等音乐模块
音频驱动视觉参数接口
```

重要原则：

```text
不要重复集成 miniaudio
不要重新写跨平台 AudioBackend
不要依赖 openFrameworks
不要做 OSC / MIDI / GUI
优先复用 TrussC 已有 sound/audio core
```

---

## 1. 当前前提

TrussC 已经有声音基础层：

```text
tc/sound/
Sound
ChipSound
miniaudio + dr_libs
audio playback / simple sound generation
```

所以 `tcxPDSP` 不负责底层音频设备。

`tcxPDSP` 的职责是：

```text
在 TrussC 已有音频系统之上，增加实时 DSP / patch / sequencing / synthesis 能力。
```

也就是：

```text
TrussC core sound = 音频设备、播放、基础声音输出
tcxPDSP = DSP graph、合成器、生成音乐、音频调制
```

---

## 2. 第一步：先检查 TrussC 现有 sound 层

AI 在写代码前，必须先检查：

```text
core/include/tc/sound/
core/src/
examples/sound/
examples/*sound*
docs/REFERENCE.md
```

重点确认：

```text
1. TrussC 当前 Sound / ChipSound API 怎么工作
2. miniaudio device 在哪里初始化
3. 是否已有 raw audio callback
4. 是否已有 AudioStream / SoundStream
5. 是否可以实时填充 float output buffer
6. 是否支持 interleaved float buffer
7. 是否已有 sampleRate / bufferSize / channels 配置
```

---

## 3. 根据检查结果决定接入方式

### 情况 A：TrussC 已经有 raw audio callback

如果已经有类似接口：

```cpp
setAudioCallback(...)
AudioStream
SoundStream
audioCallback(float* output, int frames, int channels)
```

那 `tcxPDSP` 直接复用。

示意：

```cpp
tc::AudioStream stream;
tcx::pdsp::Processor pdsp;

stream.setOutputCallback([&](float* output, int frames, int channels) {
    pdsp.process(output, frames, channels);
});
```

此时 `tcxPDSP` 不需要自己的 `AudioEngine`。

只需要一个轻量入口：

```cpp
namespace tcx::pdsp {

class Engine {
public:
    bool setupWithTrussAudio(tc::AudioStream& stream);
    PatchNode& out(int channel);
    Processor& processor();
};

}
```

---

### 情况 B：TrussC 没有 raw audio callback

如果 TrussC 目前只有：

```cpp
Sound.load(...)
Sound.play(...)
ChipSound.play(...)
```

而没有可供 addon 实时写 buffer 的 callback，那么不要在 `tcxPDSP` 里重新集成 miniaudio。

正确做法是：

**给 TrussC core 的 sound 层补一个最小 callback 接口。**

建议在 TrussC core 中增加：

```cpp
namespace tc {

struct AudioStreamSettings {
    int sampleRate = 48000;
    int bufferSize = 256;
    int outputChannels = 2;
    int inputChannels = 0;
};

class AudioStream {
public:
    bool setup(const AudioStreamSettings& settings);

    void setOutputCallback(
        std::function<void(float* output, int frames, int channels)> callback
    );

    void start();
    void stop();

    int sampleRate() const;
    int bufferSize() const;
    int outputChannels() const;
};

}
```

这个接口内部使用 TrussC core 已有的 miniaudio 封装。

`tcxPDSP` 只调用它，不直接碰 miniaudio。

---

## 4. `tcxPDSP` 的定位

`tcxPDSP` 应该是 TrussC 的高级音乐层：

```text
tcxPDSP
  core DSP graph
  patch node system
  audio processor
  oscillator / filter / envelope / effect modules
  sequencer
  synth modules
  audio analyzer
```

它不是：

```text
miniaudio wrapper
sound file player
OSC addon
MIDI addon
GUI addon
DAW
plugin host
```

---

## 5. 推荐目录结构

```text
addons/tcxPDSP/
  README.md
  LICENSE.md
  addon.json
  addon.cmake

  src/
    tcxPDSP.h

    core/
      AudioContext.h
      AudioContext.cpp

      AudioBuffer.h
      AudioBuffer.cpp

      Processor.h
      Processor.cpp

      AudioNode.h
      AudioNode.cpp

      PatchNode.h
      PatchNode.cpp

      PatchGraph.h
      PatchGraph.cpp

      Parameter.h
      Parameter.cpp

      SmoothedValue.h
      SmoothedValue.cpp

      Meter.h
      Meter.cpp

    dsp/
      Oscillator.h
      Oscillator.cpp

      SineOsc.h
      SineOsc.cpp

      Noise.h
      Noise.cpp

      Gain.h
      Gain.cpp

      EnvelopeADSR.h
      EnvelopeADSR.cpp

      LFO.h
      LFO.cpp

      FilterSVF.h
      FilterSVF.cpp

      Biquad.h
      Biquad.cpp

      Delay.h
      Delay.cpp

      Panner.h
      Panner.cpp

      Mixer.h
      Mixer.cpp

      Saturation.h
      Saturation.cpp

    modules/
      MonoSynth.h
      MonoSynth.cpp

      PolySynth.h
      PolySynth.cpp

      DrumVoice.h
      DrumVoice.cpp

      SimpleSampler.h
      SimpleSampler.cpp

      ChannelStrip.h
      ChannelStrip.cpp

    sequencer/
      Transport.h
      Transport.cpp

      EventQueue.h
      EventQueue.cpp

      Sequencer.h
      Sequencer.cpp

      StepSequencer.h
      StepSequencer.cpp

      Pattern.h
      Pattern.cpp

    analysis/
      EnvelopeFollower.h
      EnvelopeFollower.cpp

      RMS.h
      RMS.cpp

      PeakMeter.h
      PeakMeter.cpp

    utils/
      MathUtils.h
      Denormal.h
      Random.h
      RingBuffer.h
      LockFreeQueue.h

  examples/
    01_basic_sine/
    02_patch_graph/
    03_subtractive_synth/
    04_step_sequencer/
    05_audio_reactive/
    06_poly_synth/
```

---

## 6. 命名规范

Namespace：

```cpp
namespace tcx::pdsp
```

入口头文件：

```cpp
#include "tcxPDSP.h"
```

类命名：

```text
tcx::pdsp::Processor
tcx::pdsp::AudioContext
tcx::pdsp::AudioBuffer
tcx::pdsp::AudioNode
tcx::pdsp::PatchNode
tcx::pdsp::Parameter
tcx::pdsp::SmoothedValue
tcx::pdsp::SineOsc
tcx::pdsp::Gain
tcx::pdsp::FilterSVF
tcx::pdsp::ADSR
tcx::pdsp::Delay
tcx::pdsp::Mixer
tcx::pdsp::Transport
tcx::pdsp::StepSequencer
tcx::pdsp::MonoSynth
tcx::pdsp::PolySynth
```

不要使用 `ofx` 前缀。

不要把 addon 叫 `ofxPDSP port`。

---

## 7. 第一版最小目标

第一版目标：

```text
1. 复用 TrussC sound core 输出音频
2. 可以生成 sine wave
3. 可以使用 patch 连接：osc -> gain -> output
4. 可以实时修改 frequency / gain
5. 参数变化需要平滑，避免 click
6. 可以组合 oscillator + envelope + filter 做简单 synth
7. 可以用 StepSequencer 播放 16-step pattern
8. 不依赖 OSC / MIDI / GUI
9. 不直接依赖 miniaudio
10. 不依赖 openFrameworks
```

---

## 8. 最小使用方式

期望最终 API 类似：

```cpp
#include "tcxPDSP.h"

tcx::pdsp::Processor pdsp;
tcx::pdsp::SineOsc osc;
tcx::pdsp::Gain gain;

void setup() {
    pdsp.setup({
        .sampleRate = 48000,
        .bufferSize = 256,
        .outputChannels = 2
    });

    osc.setFrequency(220.0f);
    gain.setGain(0.2f);

    osc.out() >> gain.in();
    gain.out() >> pdsp.out(0);
    gain.out() >> pdsp.out(1);

    tc::AudioStream stream;
    stream.setup({
        .sampleRate = 48000,
        .bufferSize = 256,
        .outputChannels = 2
    });

    stream.setOutputCallback([&](float* output, int frames, int channels) {
        pdsp.process(output, frames, channels);
    });

    stream.start();
}
```

如果 TrussC 没有 `tc::AudioStream`，先在 core sound 层补这个最小接口。

---

## 9. AudioContext

`AudioContext` 不负责打开设备，只保存 DSP 处理所需的音频上下文。

```cpp
namespace tcx::pdsp {

struct AudioContext {
    int sampleRate = 48000;
    int bufferSize = 256;
    int outputChannels = 2;
    uint64_t currentSample = 0;

    double secondsPerSample() const {
        return 1.0 / static_cast<double>(sampleRate);
    }

    double currentTimeSeconds() const {
        return static_cast<double>(currentSample) / static_cast<double>(sampleRate);
    }
};

}
```

---

## 10. AudioBuffer

内部建议使用 planar buffer：

```text
channel 0: frame0, frame1, frame2...
channel 1: frame0, frame1, frame2...
```

输出到 TrussC audio callback 时，再由 `Processor` 写入 interleaved output：

```text
L0 R0 L1 R1 L2 R2...
```

接口：

```cpp
namespace tcx::pdsp {

class AudioBuffer {
public:
    void allocate(int channels, int frames);
    void clear();

    float* channel(int ch);
    const float* channel(int ch) const;

    float& sample(int ch, int frame);

    int channels() const;
    int frames() const;

private:
    int numChannels = 0;
    int numFrames = 0;
    std::vector<float> data;
};

}
```

要求：

```text
setup 阶段允许 allocate
audio callback 中禁止重新 allocate
```

---

## 11. AudioNode

所有 DSP module 继承 `AudioNode`。

```cpp
namespace tcx::pdsp {

class AudioNode {
public:
    virtual ~AudioNode() = default;

    virtual void prepare(AudioContext& ctx) = 0;
    virtual void process(AudioContext& ctx, int frames) = 0;

protected:
    AudioBuffer outputBuffer;
};

}
```

后续可以增加：

```cpp
bool isPrepared() const;
bool isActive() const;
void setActive(bool active);
```

---

## 12. PatchNode

参考 ofxPDSP 的模块化连接思想，保留 `>>` 语法。

```cpp
namespace tcx::pdsp {

class PatchNode {
public:
    PatchNode(AudioNode* owner = nullptr, int channel = 0);

    void connect(PatchNode& destination);
    void disconnect(PatchNode& destination);

    AudioNode* owner() const;
    int channel() const;

private:
    AudioNode* ownerNode = nullptr;
    int channelIndex = 0;
    std::vector<PatchNode*> destinations;
};

inline PatchNode& operator>>(PatchNode& source, PatchNode& destination) {
    source.connect(destination);
    return destination;
}

}
```

使用方式：

```cpp
osc.out() >> filter.in();
filter.out() >> gain.in();
gain.out() >> pdsp.out(0);
gain.out() >> pdsp.out(1);
```

---

## 13. Processor

`Processor` 是 `tcxPDSP` 的核心。  
它负责 DSP graph 的处理，并把结果写入 TrussC audio callback 提供的 output buffer。

```cpp
namespace tcx::pdsp {

struct ProcessorSettings {
    int sampleRate = 48000;
    int bufferSize = 256;
    int outputChannels = 2;
};

class Processor {
public:
    bool setup(const ProcessorSettings& settings);

    PatchNode& out(int channel);

    void process(float* interleavedOutput, int frames, int channels);

    AudioContext& context();

private:
    AudioContext ctx;

    std::vector<PatchNode> outputNodes;
    std::vector<AudioNode*> activeNodes;

    AudioBuffer mixBuffer;

    void rebuildGraph();
    void processGraph(int frames);
    void clearOutput(float* output, int frames, int channels);
};

}
```

第一版可以简单处理：

```text
所有注册进 activeNodes 的 node 每个 block 都 process
不做复杂 lazy evaluation
不做复杂 graph topological sort
```

第二版再优化：

```text
topological sorting
lazy evaluation
dirty flag
SIMD
control-rate / audio-rate 区分
```

---

## 14. Parameter / SmoothedValue

实时调参数必须平滑，避免 click。

```cpp
namespace tcx::pdsp {

class SmoothedValue {
public:
    void reset(float value);
    void setTarget(float value);
    void setTime(float milliseconds, int sampleRate);

    float next();

private:
    float currentValue = 0.0f;
    float targetValue = 0.0f;
    float step = 0.0f;
    int samplesRemaining = 0;
};

class Parameter {
public:
    explicit Parameter(float defaultValue = 0.0f);

    void set(float value);
    float getTarget() const;

    void prepare(int sampleRate, float smoothingMs = 10.0f);
    float next();

private:
    std::atomic<float> target;
    SmoothedValue smoother;
};

}
```

要求：

```text
main thread 调 set()
audio thread 调 next()
不要在 audio thread lock
```

---

## 15. 第一批 DSP 模块

### 15.1 SineOsc

```cpp
namespace tcx::pdsp {

class SineOsc : public AudioNode {
public:
    SineOsc();

    void prepare(AudioContext& ctx) override;
    void process(AudioContext& ctx, int frames) override;

    PatchNode& out();

    void setFrequency(float hz);
    void setPhase(float normalizedPhase);

private:
    Parameter frequency;
    float phase = 0.0f;
    float sampleRate = 48000.0f;

    PatchNode output;
};

}
```

---

### 15.2 Gain

```cpp
namespace tcx::pdsp {

class Gain : public AudioNode {
public:
    Gain();

    void prepare(AudioContext& ctx) override;
    void process(AudioContext& ctx, int frames) override;

    PatchNode& in();
    PatchNode& out();

    void setGain(float value);

private:
    Parameter gain;

    PatchNode input;
    PatchNode output;
};

}
```

---

### 15.3 Noise

```cpp
namespace tcx::pdsp {

class Noise : public AudioNode {
public:
    enum class Type {
        White,
        Pink
    };

    void prepare(AudioContext& ctx) override;
    void process(AudioContext& ctx, int frames) override;

    void setType(Type type);

    PatchNode& out();

private:
    Type type = Type::White;
    PatchNode output;
};

}
```

---

### 15.4 ADSR

`ADSR` 可以先不是 `AudioNode`，而是一个 envelope helper。

```cpp
namespace tcx::pdsp {

class ADSR {
public:
    void prepare(float sampleRate);

    void noteOn();
    void noteOff();

    float process();

    void setAttack(float seconds);
    void setDecay(float seconds);
    void setSustain(float value);
    void setRelease(float seconds);

    bool isActive() const;

private:
    enum class Stage {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };

    Stage stage = Stage::Idle;

    float sampleRate = 48000.0f;
    float current = 0.0f;

    float attackSeconds = 0.01f;
    float decaySeconds = 0.1f;
    float sustainLevel = 0.7f;
    float releaseSeconds = 0.2f;
};

}
```

---

### 15.5 FilterSVF

```cpp
namespace tcx::pdsp {

class FilterSVF : public AudioNode {
public:
    enum class Mode {
        LowPass,
        HighPass,
        BandPass,
        Notch
    };

    void prepare(AudioContext& ctx) override;
    void process(AudioContext& ctx, int frames) override;

    PatchNode& in();
    PatchNode& out();

    void setCutoff(float hz);
    void setResonance(float q);
    void setMode(Mode mode);

private:
    Parameter cutoff;
    Parameter resonance;
    Mode mode = Mode::LowPass;

    PatchNode input;
    PatchNode output;
};

}
```

---

### 15.6 Delay

```cpp
namespace tcx::pdsp {

class Delay : public AudioNode {
public:
    void prepare(AudioContext& ctx) override;
    void process(AudioContext& ctx, int frames) override;

    PatchNode& in();
    PatchNode& out();

    void setDelayTime(float seconds);
    void setFeedback(float value);
    void setWet(float value);

private:
    std::vector<float> delayBuffer;
    int writeIndex = 0;

    Parameter delayTime;
    Parameter feedback;
    Parameter wet;

    PatchNode input;
    PatchNode output;
};

}
```

---

### 15.7 Mixer

```cpp
namespace tcx::pdsp {

class Mixer : public AudioNode {
public:
    int addInput();

    PatchNode& in(int index);
    PatchNode& out();

    void setLevel(int index, float level);
    void setPan(int index, float pan);

    void prepare(AudioContext& ctx) override;
    void process(AudioContext& ctx, int frames) override;

private:
    struct Channel {
        PatchNode input;
        Parameter level;
        Parameter pan;
    };

    std::vector<Channel> channels;
    PatchNode output;
};

}
```

---

## 16. Sequencer：必须基于 sample time

不要用 app `update()` 帧率触发音乐事件。

Sequencer 要基于：

```text
sampleRate
currentSample
BPM
samplesPerBeat
sample offset inside current audio block
```

---

### 16.1 Transport

```cpp
namespace tcx::pdsp {

class Transport {
public:
    void prepare(int sampleRate);

    void setBpm(double bpm);
    double getBpm() const;

    void play();
    void stop();
    void reset();

    bool isPlaying() const;

    uint64_t currentSample() const;
    void advance(int frames);

    uint64_t samplesPerBeat() const;

private:
    int sampleRate = 48000;

    std::atomic<double> bpm = 120.0;
    std::atomic<bool> playing = false;

    uint64_t samplePosition = 0;
};

}
```

---

### 16.2 SequenceEvent

不要在 audio thread 里大量使用 `std::function`。

推荐事件结构：

```cpp
namespace tcx::pdsp {

enum class EventType {
    NoteOn,
    NoteOff,
    ParameterChange,
    Trigger
};

struct SequenceEvent {
    EventType type = EventType::Trigger;

    uint64_t sampleTime = 0;
    int sampleOffsetInBlock = 0;

    int targetId = -1;
    int note = 60;

    float value0 = 0.0f;
    float value1 = 0.0f;
};

}
```

---

### 16.3 EventQueue

```cpp
namespace tcx::pdsp {

class EventQueue {
public:
    bool push(const SequenceEvent& event);
    bool pop(SequenceEvent& event);

private:
    LockFreeQueue<SequenceEvent, 1024> queue;
};

}
```

第一版也可以用预分配 ring buffer。  
避免 audio thread 里 lock 和 allocation。

---

### 16.4 StepSequencer

```cpp
namespace tcx::pdsp {

class StepSequencer {
public:
    void prepare(int sampleRate);

    void setSteps(int steps);
    void setDivision(int stepsPerBar);

    void setStep(
        int index,
        bool active,
        int note,
        float velocity,
        float probability = 1.0f
    );

    void process(
        AudioContext& ctx,
        Transport& transport,
        int frames,
        EventQueue& queue
    );

private:
    struct Step {
        bool active = false;
        int note = 60;
        float velocity = 1.0f;
        float probability = 1.0f;
    };

    std::vector<Step> steps;

    int sampleRate = 48000;
    int stepsPerBar = 16;

    uint64_t nextStepSample = 0;
    int currentStep = 0;
};

}
```

---

## 17. Synth modules

第一版先做 `MonoSynth`。

### 17.1 MonoSynth

```cpp
namespace tcx::pdsp {

class MonoSynth : public AudioNode {
public:
    void prepare(AudioContext& ctx) override;
    void process(AudioContext& ctx, int frames) override;

    void noteOn(int midiNote, float velocity);
    void noteOff();

    void setFrequency(float hz);

    void setCutoff(float hz);
    void setResonance(float q);

    void setAttack(float seconds);
    void setDecay(float seconds);
    void setSustain(float value);
    void setRelease(float seconds);

    PatchNode& out();

private:
    SineOsc osc;
    FilterSVF filter;
    ADSR ampEnv;
    Gain amp;

    int currentNote = -1;
    float velocity = 1.0f;

    PatchNode output;
};

}
```

内部先简单实现，不一定必须真的用 patch graph 连接子模块。  
第一版可以在 `MonoSynth::process()` 里直接调用 oscillator / envelope / filter 的内部函数。

---

### 17.2 PolySynth

第二阶段做。

```cpp
namespace tcx::pdsp {

class PolySynth : public AudioNode {
public:
    void setVoiceCount(int count);

    void noteOn(int midiNote, float velocity);
    void noteOff(int midiNote);

    PatchNode& out();

private:
    struct Voice {
        SineOsc osc;
        ADSR env;
        FilterSVF filter;

        int note = -1;
        bool active = false;
        uint64_t age = 0;
    };

    std::vector<Voice> voices;
};

}
```

第一版固定 8 voices。

---

## 18. Audio analysis bridge

这个部分是为了让 TrussC 视觉系统读取声音数据。

### RMS

```cpp
namespace tcx::pdsp {

class RMS {
public:
    void process(const float* buffer, int frames);
    float value() const;

private:
    std::atomic<float> currentValue = 0.0f;
};

}
```

### PeakMeter

```cpp
namespace tcx::pdsp {

class PeakMeter {
public:
    void process(const float* buffer, int frames);
    float value() const;

private:
    std::atomic<float> currentValue = 0.0f;
};

}
```

### EnvelopeFollower

```cpp
namespace tcx::pdsp {

class EnvelopeFollower {
public:
    void prepare(int sampleRate);

    void setAttack(float seconds);
    void setRelease(float seconds);

    float processSample(float x);
    float value() const;

private:
    float env = 0.0f;
    float attackCoeff = 0.01f;
    float releaseCoeff = 0.001f;
};

}
```

---

## 19. Realtime audio 规则

audio callback 中禁止：

```text
new / delete
malloc / free
std::vector push_back 导致扩容
mutex lock
文件 IO
printf / cout / log
字符串拼接
加载 sample
修改复杂 graph
调用 UI
调用网络
```

audio callback 中允许：

```text
DSP 计算
读取 atomic 参数
读写预分配 AudioBuffer
从 lock-free queue pop event
写入 lock-free meter
更新 sample counter
```

---

## 20. Graph 修改策略

第一版可以规定：

```text
patch graph 只能在 setup 阶段建立
运行中不支持频繁 connect / disconnect
```

之后再做：

```text
main thread 修改 graph
构建 shadow graph
audio block 边界安全切换
```

不要第一版就做复杂实时 graph editing。

---

## 21. 第一阶段开发顺序

### Milestone 1：接入 TrussC audio callback

任务：

```text
1. 检查 TrussC sound core 是否已有 AudioStream callback。
2. 如果没有，在 TrussC core sound 层补一个最小 tc::AudioStream。
3. 不要重复引入 miniaudio。
4. 让 callback 能输出静音。
```

验收：

```text
TrussC example 能打开音频设备并输出静音。
```

---

### Milestone 2：Processor + SineOsc

任务：

```text
AudioContext
AudioBuffer
Processor
AudioNode
PatchNode
SineOsc
Gain
```

验收：

```cpp
osc.out() >> gain.in();
gain.out() >> pdsp.out(0);
gain.out() >> pdsp.out(1);
```

能听到稳定 sine wave。

---

### Milestone 3：Parameter smoothing

任务：

```text
Parameter
SmoothedValue
frequency smoothing
gain smoothing
```

验收：

```text
实时修改 frequency / gain 时没有明显 click。
```

---

### Milestone 4：基础合成

任务：

```text
Noise
ADSR
FilterSVF
Delay
Mixer
MonoSynth
```

验收：

```text
可以 noteOn / noteOff
可以做简单 subtractive synth
可以调 cutoff / resonance / envelope
```

---

### Milestone 5：StepSequencer

任务：

```text
Transport
EventQueue
StepSequencer
Pattern
```

验收：

```text
120 BPM 下 16-step pattern 稳定播放
不依赖 app update 帧率
事件基于 sample time
```

---

### Milestone 6：Audio analysis

任务：

```text
RMS
PeakMeter
EnvelopeFollower
```

验收：

```text
TrussC sketch 可以读取音频能量值，并驱动图形参数。
```

---

## 22. Examples

### `01_basic_sine`

展示：

```text
TrussC AudioStream callback
tcx::pdsp::Processor
SineOsc
Gain
Output
```

---

### `02_patch_graph`

展示：

```text
多个 oscillator
Mixer
Panner
Delay
```

---

### `03_subtractive_synth`

展示：

```text
MonoSynth
ADSR
Filter
noteOn / noteOff
```

---

### `04_step_sequencer`

展示：

```text
Transport
StepSequencer
MonoSynth
16-step pattern
```

---

### `05_audio_reactive`

展示：

```text
RMS
PeakMeter
EnvelopeFollower
声音驱动图形
```

---

### `06_poly_synth`

展示：

```text
PolySynth
voice allocation
simple chord pattern
```

---

## 23. 第一版不要做

明确不要做：

```text
不要引入 miniaudio
不要重写 audio backend
不要接 MIDI
不要接 OSC
不要写 GUI
不要写 Scope UI
不要写 VST/AU
不要写 plugin host
不要写复杂 sampler
不要做 FFT convolver
不要做 granular engine
不要做 Android / iOS 特殊优化
不要一开始做 SIMD
不要一开始做完整 lazy evaluation
```

第一版只追求：

```text
TrussC audio callback + DSP graph + 基础合成 + sample-accurate sequencing
```

---

## 24. `tcxPDSP.h` 入口文件

```cpp
#pragma once

#include "core/AudioContext.h"
#include "core/AudioBuffer.h"
#include "core/Processor.h"
#include "core/AudioNode.h"
#include "core/PatchNode.h"
#include "core/PatchGraph.h"
#include "core/Parameter.h"
#include "core/SmoothedValue.h"
#include "core/Meter.h"

#include "dsp/SineOsc.h"
#include "dsp/Oscillator.h"
#include "dsp/Noise.h"
#include "dsp/Gain.h"
#include "dsp/EnvelopeADSR.h"
#include "dsp/LFO.h"
#include "dsp/FilterSVF.h"
#include "dsp/Biquad.h"
#include "dsp/Delay.h"
#include "dsp/Panner.h"
#include "dsp/Mixer.h"
#include "dsp/Saturation.h"

#include "modules/MonoSynth.h"
#include "modules/PolySynth.h"
#include "modules/DrumVoice.h"
#include "modules/SimpleSampler.h"
#include "modules/ChannelStrip.h"

#include "sequencer/Transport.h"
#include "sequencer/EventQueue.h"
#include "sequencer/Sequencer.h"
#include "sequencer/StepSequencer.h"
#include "sequencer/Pattern.h"

#include "analysis/EnvelopeFollower.h"
#include "analysis/RMS.h"
#include "analysis/PeakMeter.h"
```

---

## 25. 最终验收标准

第一版 `tcxPDSP` 完成后，应满足：

```text
1. addon 名称为 tcxPDSP。
2. namespace 为 tcx::pdsp。
3. 不依赖 openFrameworks。
4. 不重复引入 miniaudio。
5. 复用 TrussC core sound/audio callback。
6. 如果 TrussC 没有 raw audio callback，则在 core sound 层补最小 AudioStream。
7. 可以输出 sine wave。
8. 可以用 >> patch node。
9. 可以实时平滑修改 frequency / gain。
10. 可以使用 ADSR + filter 做 MonoSynth。
11. 可以用 StepSequencer 播放 16-step pattern。
12. sequencer 基于 sample time，不依赖 app update 帧率。
13. audio callback 中没有动态内存分配。
14. 可以输出 RMS / Peak / EnvelopeFollower 给 TrussC 视觉系统使用。
15. examples 至少包含 basic_sine、patch_graph、subtractive_synth、step_sequencer、audio_reactive。
```

---

## 26. 一句话执行原则

```text
tcxPDSP 不做底层音频设备，不重复引入 miniaudio。
它复用 TrussC 已有 sound core，在其上实现 ofxPDSP 风格的 DSP graph、patch、synth、sequencer 和 audio-reactive 能力。
```
