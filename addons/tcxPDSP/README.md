# tcxPDSP — TrussC 实时数字音频信号处理 / 合成音乐 Addon

## 目录

- [1. 简介](#1-简介)
- [2. 合成音乐与 DSP 基础](#2-合成音乐与-dsp-基础)
- [3. 架构总览](#3-架构总览)
- [4. 线程模型](#4-线程模型)
- [5. 核心层 (core/)](#5-核心层-core)
- [6. DSP 模块 (dsp/)](#6-dsp-模块-dsp)
- [7. 合成器模块 (modules/)](#7-合成器模块-modules)
- [8. 音序器 (sequencer/)](#8-音序器-sequencer)
- [9. 音频分析 (analysis/)](#9-音频分析-analysis)
- [10. 工具 (utils/)](#10-工具-utils)
- [11. 快速开始](#11-快速开始)
- [12. 范例说明](#12-范例说明)
- [13. 实时音频规则](#13-实时音频规则)

---

## 1. 简介

`tcxPDSP` 是 TrussC 框架的实时 **数字信号处理 (DSP)** 与 **合成音乐** addon。

它可以做什么：

- 生成正弦波、锯齿波、方波、三角波等基础波形
- 实时修改频率、音量，参数平滑无杂音
- 用 ADSR 包络 + 滤波器 搭建减法合成器
- 16 步步进音序器，按 BPM 自动演奏
- 延迟、失真、混音等效果处理
- 将音频能量 (RMS/Peak) 反馈给 TrussC 图形系统，实现音画互动

不做什么：

- 不依赖 openFrameworks / JUCE 等框架
- 不管理底层音频设备（通过 TrussC AudioEngine output callback 复用 TrussC 音频设备）
- 不提供 GUI / MIDI / OSC 接口

依赖与编译：

- addon 内部没有第三方下载步骤，也不包含额外静态库/动态库
- 音频设备、平台后端与 miniaudio 都由 TrussC core 统一提供
- 新项目引用 `addons/tcxPDSP` 时只编译本 addon 源码；不存在每个项目重复下载第三方库的问题
- 如果后续增加第三方 DSP/FFT 库，应放到平台级缓存或预构建路径，避免每个 app 工程重复下载

---

## 2. 合成音乐与 DSP 基础

### 声音的本质

声音是空气的振动。数字音频将振动转换为每秒数万次采样：

```
采样率 (Sample Rate) = 每秒采样次数，通常 44100 或 48000 Hz
每帧 (Frame) = 一次采样时刻所有声道的数据
音频块 (Audio Block) = 一次 callback 处理的帧数，例如 256 帧
```

### 振荡器 (Oscillator)

振荡器是合成器的"声带"。它生成基础波形：

| 波形 | 音色 | 类名 |
|------|------|------|
| 正弦波 | 纯净、柔和 | `SineOsc` / `SineOscillator` |
| 三角波 | 稍温暖 | `TriangleOscillator` |
| 锯齿波 | 明亮、锋利 | `SawOscillator` |
| 方波 | 空洞、"8-bit" | `SquareOscillator` |
| 噪声 | 嘶嘶声 | `Noise` (白噪/粉噪) |

### 滤波器 (Filter)

滤波器像"音色雕刻刀"，切除某些频率成分：

| 类型 | 效果 | 类名 |
|------|------|------|
| 低通 (LowPass) | 去掉高频，声音变闷 | `FilterSVF`, `Biquad` |
| 高通 (HighPass) | 去掉低频，声音变薄 | `FilterSVF`, `Biquad` |
| 带通 (BandPass) | 只保留中间一段 | `FilterSVF`, `Biquad` |

参数：

- **Cutoff (截止频率)**：滤除的边界频率
- **Resonance / Q (共振)**：截止点附近的强调量

### 包络 (Envelope)

包络控制声音的"形状"——起音、衰减、保持、释音 (ADSR)：

```
音量
1.0 ┤     /\___________
    │    /              \
    │   /                \
0.0 ┤──/                  \──
    Attack Decay Sustain Release
    ←A→  ←D→  ←──S──→  ←R→
```

| 阶段 | 含义 | 典型值 |
|------|------|--------|
| Attack | 从 0 到峰值的上升时间 | 0.01~0.1 秒 |
| Decay | 从峰值降到保持电平 | 0.1~0.5 秒 |
| Sustain | 按住键时的稳定音量 | 0.3~0.8 |
| Release | 松开键后的衰减 | 0.1~1.0 秒 |

### 低频振荡器 (LFO)

LFO 是频率很低的振荡器（通常 0.1~20 Hz），本身听不到，用来**调制**其他参数。例如：LFO → 滤波器的 cutoff → 产生"哇音"效果。

### 信号链路 (Signal Chain)

典型的减法合成器链路：

```
Oscillator → Filter → Amplifier (ADSR) → Output
  音源         音色         音量             输出
```

---

## 3. 架构总览

```
┌─────────────────────────────────────────────────────────┐
│                      用户代码 (main thread)               │
│  setup(): 创建 AudioStream, 配置节点, 建图, start()       │
│  update()/draw(): 读 RMS/Peak, 驱动图形                   │
│  keyPressed(): setParameter() 修改参数                    │
└──────────────────────┬──────────────────────────────────┘
                       │ setCallback(lambda)
                       ▼
┌─────────────────────────────────────────────────────────┐
│              AudioStream (TrussC AudioEngine callback)    │
│  callback(float* output, int frames, int channels)        │
│  ┌──────────────────────────────────────────────────┐   │
│  │  for each AudioNode:                              │   │
│  │    node.process(ctx, frames)                      │   │
│  │       │                                            │   │
│  │       ├── Oscillator: fill outputBuffer            │   │
│  │       ├── Filter:    read→process→write in-place   │   │
│  │       ├── Gain:      multiply by Parameter.next()  │   │
│  │       ├── ADSR:      envelope.process() per sample │   │
│  │       └── Mixer:     sum channels with pan law     │   │
│  │                                                     │   │
│  │  Write final mix → interleaved output buffer        │   │
│  └──────────────────────────────────────────────────┘   │
│  RMS.process(samples) → atomic.store()                   │
└─────────────────────────────────────────────────────────┘
                       │ atomic.load()
                       ▼
┌─────────────────────────────────────────────────────────┐
│              分析桥 (main thread 读取)                     │
│  RMS::value() → 实时音量 → 驱动图形大小/颜色               │
│  PeakMeter::value() → 峰值 → 驱动图形高度                 │
│  EnvelopeFollower::value() → 包络跟踪                     │
└─────────────────────────────────────────────────────────┘
```

### 目录结构

```
src/
├── tcxPDSP.h              ← 统一入口，include 这一个即可
├── core/                  ← 基础设施
│   ├── AudioContext.h     采样率/缓冲区/时间上下文
│   ├── AudioBuffer.h      Planar float buffer (声道优先布局)
│   ├── AudioNode.h        所有 DSP 模块的基类
│   ├── PatchNode.h        >> 连接语法
│   ├── Processor.h        DSP graph 引擎
│   ├── Parameter.h        线程安全参数 (atomic + 平滑)
│   ├── SmoothedValue.h    线性 ramp (消除杂音)
│   └── AudioStream.h/.cpp TrussC AudioEngine 输出封装
├── dsp/                   ← 基础信号处理模块
│   ├── Oscillator.h       振荡器基类 + 4 波形
│   ├── SineOsc.h          正弦振荡器 (独立版)
│   ├── Gain.h             音量控制
│   ├── Noise.h            白噪声 / 粉红噪声
│   ├── ADSR.h             包络发生器
│   ├── FilterSVF.h        状态变量滤波器 (LP/HP/BP/Notch)
│   ├── Biquad.h           双二阶滤波器 (7 种模式, RBJ Cookbook)
│   ├── Delay.h            延迟效果器
│   ├── Mixer.h            多通道混音器 (固定功率 pan 律)
│   ├── Panner.h           立体声声像
│   ├── Saturation.h       软饱和/失真
│   └── LFO.h              低频振荡器 (4 波形)
├── modules/               ← 合成器 / 乐器模块
│   ├── MonoSynth.h        单音减法合成器
│   ├── PolySynth.h        多音合成器 (8 复音, voice stealing)
│   ├── DrumVoice.h        鼓音色 (Kick/Snare/Hi-Hat)
│   ├── SimpleSampler.h    采样播放器
│   └── ChannelStrip.h     通道条 (Gain → EQ → Pan)
├── sequencer/             ← 音序器
│   ├── Transport.h        基于采样时间的音乐时钟
│   ├── EventQueue.h       Lock-free SPSC 事件队列
│   ├── StepSequencer.h    16 步步进音序器
│   ├── Sequencer.h        时间线音序器
│   └── Pattern.h          乐句 / 和弦容器
├── analysis/              ← 音频分析 (音画互动桥)
│   ├── RMS.h              均方根能量
│   ├── PeakMeter.h        峰值检测
│   └── EnvelopeFollower.h 包络跟踪
└── utils/                 ← 工具
    ├── MathUtils.h         MIDI↔Hz, dB↔gain, map/lerp/clamp
    ├── Denormal.h          非规格化数防护
    ├── Random.h            Xorshift32 快速随机
    ├── RingBuffer.h        SPSC 环形缓冲
    └── LockFreeQueue.h     通用 lock-free 队列
```

---

## 4. 线程模型

```
┌──────────────────┐     atomic<float>     ┌──────────────────┐
│   主线程          │ ──────────────────→  │   音频线程         │
│ (main thread)    │    Parameter::set()   │ (audio callback)  │
│                  │                       │                   │
│ setup()          │                       │ process()         │
│ setFrequency()   │                       │ Parameter::next() │
│ setGain()        │                       │                   │
│ keyPressed()     │ ←──────────────────   │ RMS::process()    │
│                  │    atomic<float>      │ PeakMeter::       │
│ draw()           │    value() 读取        │   process()       │
└──────────────────┘                       └──────────────────┘
```

关键规则：

- **音频线程不做的事**：`new/delete`, `malloc/free`, 文件 IO, mutex lock, `printf`
- **音频线程做的事**：DSP 计算、读 atomic 参数、写 atomic meter、更新计数器
- **参数平滑**：`Parameter::set()` 主线程写 atomic target → `next()` 音频线程读 + SmoothedValue 线性插值
- **事件队列**：LockFreeQueue SPSC，sequencer 音频线程 push → callback pop

---

## 5. 核心层 (core/)

### AudioStream — 音频输出

```cpp
tcx::pdsp::AudioStream stream;
tcx::pdsp::AudioStreamSettings cfg;
cfg.sampleRate = 48000;
cfg.bufferSize = 256;
cfg.outputChannels = 2;

stream.setup(cfg);
stream.setCallback([](float* output, int frames, int channels) {
    // 填充 output: output[frame * channels + ch] = sample;
});
stream.start();
```

### AudioContext — 音频上下文

```cpp
struct AudioContext {
    int sampleRate;       // 48000
    int bufferSize;       // 256
    int outputChannels;   // 2
    uint64_t currentSample; // 全局采样计数
};
```

每个 AudioNode 的 `prepare(AudioContext&)` 从这里获取采样率来分配缓冲区。

### AudioBuffer — Planar 缓冲区

声道优先布局（非交错）：

```
channel 0: [f0, f1, f2, ..., fN-1]
channel 1: [f0, f1, f2, ..., fN-1]
```

```cpp
AudioBuffer buf;
buf.allocate(2, 256);      // 2 声道, 256 帧
buf.sample(0, 10) = 0.5f;  // 声道 0, 第 10 帧
float* ch1 = buf.channel(1);
```

### Parameter / SmoothedValue — 无杂音参数

```cpp
Parameter freq{440.0f};
freq.prepare(48000, 5.0f);  // 5ms 平滑
freq.set(880.0f);             // 主线程设置
float v = freq.next();        // 音频线程取值 (平滑过渡)
```

### PatchNode — 连接语法

```cpp
PatchNode& operator>>(PatchNode& src, PatchNode& dst);
// 用法: osc.out() >> gain.in();
```

---

## 6. DSP 模块 (dsp/)

### 振荡器

```cpp
SineOsc osc;
osc.prepare(ctx);
osc.setFrequency(440.0f);
osc.process(ctx, 256);
float* samples = osc.output().channel(0);

// 或用 Oscillator 基类:
SineOscillator osc2;
osc2.setFrequency(220.0f);
```

### 增益

```cpp
Gain gain;
gain.prepare(ctx);
gain.setGain(0.5f);  // -6dB
```

### ADSR 包络

```cpp
ADSR env;
env.setSampleRate(48000);
env.setAttack(0.02f);
env.setDecay(0.15f);
env.setSustain(0.6f);
env.setRelease(0.3f);

env.noteOn();
for (int i = 0; i < frames; i++) {
    float amp = env.process();
    out[i] = oscOut[i] * amp;
}
env.noteOff();
```

### 滤波器

```cpp
// 状态变量滤波器 (简单)
FilterSVF filter;
filter.prepare(ctx);
filter.setCutoff(2000.0f);
filter.setResonance(1.5f);
filter.setMode(FilterSVF::Mode::LowPass);

// 双二阶滤波器 (RBJ Cookbook, 7 种模式)
Biquad eq;
eq.prepare(ctx);
eq.setType(Biquad::Type::Peak);
eq.setFrequency(1000.0f);
eq.setQ(2.0f);
eq.setGain(6.0f);  // +6dB boost
```

### 延迟

```cpp
Delay delay;
delay.prepare(ctx);
delay.setDelayTime(0.25f);   // 250ms
delay.setFeedback(0.3f);     // 30% 反馈
delay.setWet(0.4f);          // 40% 湿信号
```

### 混音器

```cpp
Mixer mixer;
int ch0 = mixer.addInput();
int ch1 = mixer.addInput();
mixer.prepare(ctx);
mixer.setLevel(ch0, 0.8f);
mixer.setPan(ch0, -0.5f);   // 偏左
mixer.setLevel(ch1, 0.6f);
mixer.setPan(ch1, 0.5f);    // 偏右

// 填充输入缓冲 → mixer.process()
float* in0 = mixer.inputBuffer(ch0).channel(0);
// ... 写入数据 ...
mixer.process(ctx, 256);
float* outL = mixer.output().channel(0);
float* outR = mixer.output().channel(1);
```

### LFO (低频振荡器)

```cpp
LFO lfo;
lfo.prepare(ctx);
lfo.setFrequency(2.0f);        // 2 Hz 调制
lfo.setWave(LFO::Wave::Sine);
lfo.process(ctx, 256);
float* mod = lfo.output().channel(0);  // [-1, 1] 调制信号
```

---

## 7. 合成器模块 (modules/)

### MonoSynth — 单音减法合成器

```
内部链路: Oscillator → (×ADSR) → FilterSVF → Output
```

```cpp
MonoSynth synth;
synth.prepare(ctx);
synth.setAttack(0.02f);
synth.setDecay(0.15f);
synth.setSustain(0.6f);
synth.setRelease(0.3f);
synth.setCutoff(2000.0f);
synth.setResonance(0.5f);

synth.noteOn(60, 1.0f);  // MIDI 60 = C4, 力度 1.0
// ... 若干帧后 ...
synth.noteOff();
```

### PolySynth — 多音合成器 (8 复音)

```
每个 Voice: Oscillator → FilterSVF → (×ADSR)
Voice stealing: 优先复用同音高 voice，其次老化最久的
```

```cpp
PolySynth poly(8);  // 8 复音
poly.prepare(ctx);
poly.setCutoff(3000);

poly.noteOn(60, 1.0f);  // C4
poly.noteOn(64, 0.8f);  // E4 (同时发声)
poly.noteOn(67, 0.8f);  // G4 (C 大三和弦)
poly.noteOff(60);       // 释放 C
```

### DrumVoice — 鼓音色

```cpp
DrumVoice drum;
drum.prepare(ctx);
drum.trigger(DrumVoice::Kick);    // 底鼓
drum.trigger(DrumVoice::Snare);   // 军鼓
drum.trigger(DrumVoice::Hihat);   // 踩镲
```

### SimpleSampler — 采样播放

```cpp
SimpleSampler sampler;
sampler.prepare(ctx);
sampler.loadSamples(mySampleData, sampleCount);
sampler.setSpeed(1.0f);  // 正常速度
sampler.setLoop(true);
sampler.noteOn();
```

### ChannelStrip — 通道条

```
内部: Gain → Biquad EQ → Panner
```

```cpp
ChannelStrip strip;
strip.prepare(ctx);
strip.getGain().setGain(0.8f);
strip.getEQ().setFrequency(1000);  // EQ 调整
strip.getPanner().setPan(-0.3f);   // 偏左
```

---

## 8. 音序器 (sequencer/)

### Transport — 音乐时钟

基于采样时间，不依赖帧率：

```cpp
Transport transport;
transport.prepare(48000);
transport.setBpm(120.0);
transport.play();

// 在音频回调中:
transport.advance(frames);
uint64_t pos = transport.currentSample();
```

### StepSequencer — 步进音序器

每步 = 1/4 拍，BPM=120 时每步 0.125 秒：

```cpp
StepSequencer seq;
seq.prepare(48000);

// 编程一个 C 大调上行音阶
int notes[] = {60, 62, 64, 65, 67, 69, 71, 72, 0,0,0,0,0,0,0,0};
for (int i = 0; i < 16; i++)
    seq.setStep(i, notes[i] > 0, notes[i], 0.8f);

EventQueue<256> queue;

// 在音频回调中:
seq.process(ctx, transport, frames, queue);
SequenceEvent ev;
while (queue.pop(ev)) {
    if (ev.type == EventType::NoteOn)
        synth.noteOn(ev.note, ev.value0);
    else if (ev.type == EventType::NoteOff)
        synth.noteOff();
}
```

### Pattern — 乐句

```cpp
Pattern chord;
chord.addNote(60, 1.0f, 0.0f);   // C, 第 0 拍
chord.addNote(64, 0.8f, 0.0f);   // E
chord.addNote(67, 0.8f, 0.0f);   // G
chord.addNote(62, 1.0f, 4.0f);   // D, 第 4 拍

chord.schedule(transport, queue, barStartSample);
```

---

## 9. 音频分析 (analysis/)

### RMS / PeakMeter — 音画互动桥

音频线程写入 → 主线程读取（atomic, 无锁）：

```cpp
RMS rms;
PeakMeter peak;

// 音频回调中:
rms.process(audioBuffer, frames);
peak.process(audioBuffer, frames);

// 主线程 draw() 中:
float energy = rms.value();      // 0~1
float peakVal = peak.value();    // 0~1
// 用来驱动图形大小、颜色、位置
```

### EnvelopeFollower

跟踪音频包络，有独立的 attack/release 时间：

```cpp
EnvelopeFollower env;
env.prepare(48000);
env.setAttack(0.01f);
env.setRelease(0.2f);

// 音频回调中每采样:
float envelope = env.processSample(sample);

// 主线程读取:
float val = env.value();
```

---

## 10. 工具 (utils/)

### MathUtils

```cpp
using namespace tcx::pdsp::math;
float hz = midiToHz(69);            // 440.0
float midi = hzToMidi(261.6256f);   // 60.0
float gain = dbToGain(-6.0f);       // 0.5
float db = gainToDb(0.5f);          // -6.0
float ratio = semitoneToRatio(7);   // 1.498 (纯五度)
float dur = bpmToBeatDuration(120); // 0.5 秒
uint64_t spb = bpmToSamplesPerBeat(120, 48000);  // 24000
```

### Random (Xorshift32)

```cpp
Random rng;
float v = rng.next();  // [0, 1) 均匀分布
```

---

## 11. 快速开始

最简示例 — 440Hz 正弦波：

```cpp
#include <TrussC.h>
#include <tcxPDSP.h>
using namespace std;
using namespace tc;
using namespace tcx::pdsp;

class MyApp : public App {
    AudioStream stream;
    SineOsc osc;

public:
    void setup() override {
        AudioStreamSettings cfg;
        cfg.sampleRate = 48000;
        cfg.bufferSize = 256;
        cfg.outputChannels = 2;
        stream.setup(cfg);

        osc.prepare(stream.context());
        osc.setFrequency(440.0f);

        stream.setCallback([&](float* out, int frames, int ch) {
            osc.process(stream.context(), frames);
            float* s = osc.output().channel(0);
            for (int i = 0; i < frames; i++) {
                out[i * 2]     = s[i] * 0.15f;
                out[i * 2 + 1] = s[i] * 0.15f;
            }
        });
        stream.start();
    }

    void keyPressed(int key) override {
        if (key == KEY_UP)   osc.setFrequency(osc.getFrequency() + 10);
        if (key == KEY_DOWN) osc.setFrequency(osc.getFrequency() - 10);
    }
};

int main() {
    WindowSettings ws;
    ws.setSize(400, 200).setTitle("My Synth");
    return TC_RUN_APP(MyApp, ws);
}
```

---

## 12. 范例说明

| 编号 | 文件夹 | 演示内容 |
|------|--------|----------|
| 01 | `01_basic_sine` | 正弦波输出，上下键调频 |
| 02 | `02_patch_graph` | 双振荡器 + 噪声 + 延迟 |
| 03 | `03_subtractive_synth` | MonoSynth + 键盘映射 (A-K 键) |
| 04 | `04_step_sequencer` | 16 步音序器自动演奏 |
| 05 | `05_audio_reactive` | 音频驱动图形 (RMS/Peak) |
| 06 | `06_poly_synth` | 8 复音多音合成器 + 和弦 |

---

## 13. 实时音频规则

音频回调中**禁止**的操作：

```
❌ new / delete / malloc / free
❌ std::vector::push_back (可能触发扩容)
❌ mutex::lock
❌ 文件 IO / printf / cout
❌ 字符串拼接 (std::string)
❌ 加载音频文件 / 采样
❌ 修改复杂 graph 结构
```

音频回调中**允许**的操作：

```
✅ 读写预分配的 AudioBuffer
✅ DSP 数学运算 (sin, cos, tanh, sqrt, pow)
✅ std::atomic load / store (relaxed)
✅ LockFreeQueue push / pop
✅ 更新采样计数器
✅ RMS / PeakMeter process
```

**参数修改的唯一方式**：主线程 `Parameter::set()` → 音频线程 `Parameter::next()`

---

## 已知问题

### macOS 26 双音频设备冲突

TrussC 框架内置的 `tc::Sound` 系统已经打开了一个 miniaudio 播放设备（`AudioEngine` 单例）。
当 tcxPDSP 的 `AudioStream` 再创建第二个独立的 miniaudio 设备时，macOS 26（Sequoia）上可能出现：

- CoreAudio 堆损坏（`malloc_zone_error`）
- ObjC 运行时崩溃（`objc_autoreleasePoolPush` / `objc_loadWeakRetained`）
- 随机 SIGABRT / SIGSEGV

**触发条件**：同时使用 TrussC `tc::Sound` 和 tcxPDSP `AudioStream` 的场景。

**影响模块**：`Reverb` 等大内存 DSP 模块会加剧此问题（内存布局变化触发 CoreAudio 内部 bug）。

**规避方案**：
1. 不要在同一进程中同时使用 `tc::Sound` 和 `tcx::pdsp::AudioStream`
2. 如果 TrussC 未来开放 raw audio callback 接口，tcxPDSP 将改为复用而非创建新设备

### Reverb 模块

`dsp/Reverb.h` 在 macOS 26 + 双设备场景下可能触发崩溃。在单设备场景（不加载 TrussC Sound）或 Linux/Windows 上正常。
如需稳定混响效果，可考虑使用 `dsp/Delay.h` + `dsp/AllPass` 组合替代。

---

> 基于 Jeffrey Traer Bernstein 的 ofxPDSP 设计理念，为 TrussC 框架原生实现。
> 不依赖 openFrameworks / JUCE / miniaudio 重复集成。
