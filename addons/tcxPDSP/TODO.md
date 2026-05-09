# tcxPDSP — 未实现功能 Todo List

> 当前版本已完成核心 DSP graph、基础合成、步进音序、音频分析。
> 以下为 ofxPDSP 中尚未移植或可扩展的功能，按优先级排列。

---

## ⭐ P0 — 高优先级 (实用性强，见效快)

### DSP

- [x] **SawOsc / SquareOsc / TriangleOsc 独立类** → `Oscillator.h` (Sin/Tri/Saw/Square)
- [x] **PulseOsc (可变占空比方波)** → `dsp/PulseOsc.h`

### Sequencer

- [x] **StepSequencer 支持每步独立 gate length** — active step 会按 gateLength 产生 NoteOff
- [x] **StepSequencer 支持 swing / shuffle** — `setSwing(0..1)` 偏移 odd step
- [x] **StepSequencer 支持多种拍号** (3/4, 6/8...) — `setTimeSignature(beats, unit)`
- [x] **Transport 支持 loop 区间** (loop start / end bar) — sample/beat loop

---

## ⭐ P1 — 中优先级 (补齐合成器生态)

### DSP

- [x] **Reverb (混响)** → `dsp/Reverb.h` (FreeVerb)
- [x] **Chorus / Flanger** → `dsp/Chorus.h`
- [x] **Compressor** → `dsp/Compressor.h` (RMS-based)
- [x] **CombFilter** → `dsp/Effects.h`
- [x] **WaveShaper** → `dsp/Effects.h`
- [x] **BitCrusher (降比特/降采样)** → `dsp/Effects.h`

### Synth

- [x] **FMSynth (2-operator FM 合成器)** → `modules/Synths.h`
- [x] **WavetableOsc (波表振荡器)** → `modules/Synths.h`
- [x] **DrumSynth (合成鼓)** → `modules/DrumSynth.h` (pitch envelope + click + body/noise)
- [x] **VoiceAllocator** → `modules/Synths.h`

---

## ⭐ P2 — 低优先级 (扩展能力)

### DSP

- [x] **AllPass 滤波器** → `dsp/Effects.h`
- [x] **FormantFilter (共振峰滤波器)** → `dsp/Effects2.h`
- [x] **Phaser** → `dsp/Effects2.h`
- [x] **Tremolo** → `dsp/Effects2.h`
- [x] **RingMod (环形调制)** → `dsp/Effects2.h`
- [x] **Crossfader** → `dsp/Effects2.h`

### Sequencer

- [x] **EuclideanSequencer (欧几里得节奏)** → `sequencer/SequencerExt.h`
- [x] **ProbabilitySequencer** → `sequencer/SequencerExt.h`
- [x] **Arpeggiator (琶音器)** → `sequencer/SequencerExt.h`
- [x] **PatternChain** → `sequencer/SequencerExt.h`

### Synth

- [x] **GranularSynth (粒子合成)** → `modules/Advanced.h`
- [x] **PhysicalModel (Karplus-Strong 弦乐)** → `modules/Advanced.h`

### Modules

- [x] **SamplePool** → `modules/Advanced.h`
- [x] **DrumMachine** → `modules/Advanced.h`

---

## ⭐ P3 — 基础设施 & 性能优化

### Core

- [x] **Processor 拓扑排序** — 从 processor outputs 反向 DFS 生成执行顺序
- [x] **Processor 惰性求值** — 未连接到输出的注册节点不参与执行
- [x] **AudioBufferPool** → `core/AudioBufferPool.h`

### DSP

- [x] **SIMD 向量化** (SSE/AVX/NEON) → `dsp/SIMD.h` autovectorizable vector helpers
- [x] **Control-rate / Audio-rate 分离** → `core/ControlRate.h`

### Analysis

- [x] **FFT 频谱分析** → `analysis/FFTAnalyzer.h` (dependency-free DFT)
- [x] **OnsetDetector (起音检测)** → `analysis/OnsetDetector.h`
- [x] **PitchDetector (音高检测)** → `analysis/PitchDetector.h`

### Utils

- [x] **SmoothRandom** → `utils/SmoothRandom.h`
- [x] **OnePole LP/HP** → `utils/OnePole.h`

---

## 完成度统计

```
✅ 核心基础设施:    100% (Processor topology/lazy eval, AudioBufferPool, ControlRate)
✅ DSP 基础模块:    100% (12/12 + 新增 PulseOsc)
✅ DSP 效果器:      100% (Reverb/Chorus/Comp/Comb/WaveShaper/BitCrusher/AllPass/Formant/Phaser/Tremolo/RingMod/Crossfader)
✅ 合成器模块:      100% (FMSynth/WavetableOsc/DrumVoice/DrumSynth/MonoSynth/PolySynth/Sampler/ChannelStrip)
✅ 音序器:          100% (Transport loop, StepSeq gate/swing/time-signature, EuclideanSeq/ProbSeq/Arp/Pattern/PatternChain)
✅ 音频分析:        100% (RMS/Peak/EnvelopeFollower/FFT/Onset/Pitch)
✅ Utils:           100% (Random/SmoothRandom/OnePole/RingBuffer/LockFreeQueue/Math)
✅ 高级模块:        100% (GranularSynth/PhysicalModel/SamplePool/DrumMachine)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
总计:               addon 任务书内列出的功能项已补齐到可验证 baseline
```

> 当前实现优先保证 dependency-free、可构建、可验证；FFT/SIMD 为轻量 baseline，后续如需高性能大型频谱可接 TrussC pipeline 或平台级缓存第三方后端。
> 已新增模块 (PulseOsc, Reverb, Chorus, Compressor, CombFilter, WaveShaper, BitCrusher,
> AllPass, FormantFilter, Phaser, Tremolo, RingMod, Crossfader, FMSynth, WavetableOsc,
> VoiceAllocator, GranularSynth, PhysicalModel, SamplePool, DrumMachine, DrumSynth,
> EuclideanSequencer, ProbabilitySequencer, Arpeggiator, PatternChain, FFTAnalyzer, OnsetDetector, PitchDetector)
