# Codex AI 工程任务书：将 ofxFlowTools / PixelFlow 迁移为 TrussC 跨平台 addon

> 文件用途：把本文作为 Codex AI 的执行任务书，放入 TrussC 仓库上下文后，让 Codex 按阶段完成 `tcxFlowTools` addon 的设计、实现、测试与文档。
>
> 生成日期：2026-05-09
> 目标仓库：`TrussC-org/TrussC`
> 目标 addon：`addons/tcxFlowTools/`
> 建议命名空间：`tcx::flow`

---

## 1. Codex 角色设定

你是一个资深 C++20 / GPU shader / creative coding / cross-platform graphics 工程师。你的任务是在现有 TrussC 仓库中实现一个新的跨平台 addon：

```text
addons/tcxFlowTools/
```

该 addon 的核心目标是把 `ofxFlowTools` 的流体模拟、optical flow、bridge、visualization、extensions 等概念迁移到 TrussC，同时结合 `PixelFlow` 中更完整的 GPU 流体、光流、滤镜、flow-field particles、streamlines 等算法体系，最终形成一个可编译、可运行、可扩展、可维护的 TrussC addon。

这不是简单把 openFrameworks 代码包一层，也不是只把 GLSL 文件搬过来。你需要在 TrussC / sokol 的跨平台渲染模型下重新组织 API、shader、GPU 资源、example、CMake 与文档。

---

## 2. 主要参考来源

### 2.1 ofxFlowTools

仓库：

```text
https://github.com/moostrik/ofxFlowTools
```

重点：

- openFrameworks addon。
- 组合 2D fluid simulation、optical flow 与 GLSL shader。
- 主要面向 live camera input、interactive visuals、audiovisual installation。
- 依赖 ofxGui，但迁移后 TrussC addon 核心不能依赖 GUI。
- README 说明建议使用独立显卡。
- MIT License。
- README credits 中提到 fluid shader 来源包括 Mark J. Harris、PixelFlow / Thomas Diewald 等；optical flow shader 受 Quartz Composer patch 与 ofxMIOFlowGLSL 启发。
- README TODO 中提到需要 rebuild particleFlow，以及未来可能添加 Turing patterns。

必须研究的分支：

```text
master
HD
```

`HD` 分支不是可忽略项。它比 master 包含更多提交，并包含 `example_simple` 等目录。必须把 `master` 和 `HD` 做差异分析，尤其关注 shader、buffer format、resolution handling、example、API / 参数变化。

### 2.2 PixelFlow

仓库：

```text
https://github.com/diwi/PixelFlow
```

重点：

- Processing / Java 生态中的 high performance GPU-Computing / GLSL 库。
- 包含 Fluid Simulation、Flow Field Visualisation、Streamlines、Flow Field Particles、Optical Flow、PostProcessing Filters、LIC / Streamlines 等。
- 支持 Windows、Linux、MacOSX。
- MIT License。
- 虽然它是 Java / Processing 库，但算法结构、shader pass、filter pipeline、fluid/optical-flow API 对本 addon 很有参考价值。

### 2.3 TrussC

仓库：

```text
https://github.com/TrussC-org/TrussC
```

重点：

- C++20 creative coding framework。
- 基于 sokol。
- API 风格受 openFrameworks 启发。
- 主 include 为 `TrussC.h`。
- 使用 CMake preset 构建 macOS、Windows、Linux、Web 等平台。
- TrussC 已有 addon，例如 `addons/tcxImGui/`，其结构包含：

```text
example-basic/
src/
CMakeLists.txt
README.md
addon.json
```

必须优先参考现有 TrussC addon 的实际 CMake、include、example、addon.json 写法，而不是假设 openFrameworks addon 结构可直接套用。

---

## 3. 总体目标

实现一个名为 `tcxFlowTools` 的 TrussC addon，提供以下核心能力：

1. **2D GPU Fluid Simulation**
   - velocity / density / temperature / pressure / divergence。
   - advection。
   - Jacobi pressure solve。
   - gradient subtraction / projection。
   - vorticity confinement。
   - dissipation。
   - buoyancy。
   - optional viscosity / diffusion。
   - optional obstacles / boundaries。
   - 支持外部输入注入 velocity、density、temperature。

2. **Optical Flow**
   - 基于前后帧 texture 计算运动场。
   - 支持 luminance conversion、blur / smoothing、threshold、decay、strength、scale、temporal smoothing。
   - 输出 flow texture。
   - flow texture 可以直接驱动 fluid velocity。

3. **Bridge Flow System**
   - 把 camera / video / texture / mouse / procedural input 转换为可注入 fluid 的 velocity、density、temperature。
   - 至少包含：
     - `VelocityBridge`
     - `DensityBridge`
     - `TemperatureBridge`
     - `CombinedBridge`
   - 对应 ofxFlowTools 中 `core/bridge` 模块概念。

4. **Visualization**
   - density visualizer。
   - velocity color visualizer。
   - velocity vector field visualizer。
   - pressure visualizer。
   - temperature visualizer。
   - optional LIC / streamlines / arrows。

5. **Extensions**
   - mouse flow。
   - average flow / watcher。
   - split velocity。
   - particle flow。
   - HD / high-resolution input pipeline。
   - optional Turing patterns。

6. **Examples and Docs**
   - 至少 6 个 example。
   - README、license attribution、migration report、HD notes、portability notes、known limitations。

---

## 4. 硬性约束

### 4.1 语言与构建

- 使用 C++20。
- addon 必须作为 TrussC addon 集成。
- 使用当前 TrussC 仓库中的 CMake / addon 规范。
- 目录中必须包含：

```text
addon.json
CMakeLists.txt
README.md
LICENSES_THIRD_PARTY.md
src/
shaders/
examples 或 example-* 目录
tests/
```

- CMake 需要以当前 TrussC 推荐方式链接核心库。可参考 `addons/tcxImGui/CMakeLists.txt`，其模式类似：

```cmake
cmake_minimum_required(VERSION 3.20)
message(STATUS "[tcxFlowTools] Loading addon...")

file(GLOB_RECURSE TCX_FLOWTOOLS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

add_library(tcxFlowTools STATIC ${TCX_FLOWTOOLS_SOURCES})
target_include_directories(tcxFlowTools PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(tcxFlowTools PUBLIC TrussC)

message(STATUS "[tcxFlowTools] Addon loaded")
```

实际代码应以 TrussC 仓库当前 CMake 约定为准。

### 4.2 跨平台

必须避免依赖 OpenGL-only API。

不要直接使用：

```text
glBindTexture
glFramebufferTexture2D
gl_FragColor
OpenGL extension
openFrameworks ofFbo / ofShader / ofTexture
Processing / Java API
```

shader 必须迁移为 TrussC / sokol 可接受的形式。

必须考虑：

- Metal。
- D3D。
- Web / Emscripten。
- OpenGL fallback。
- texture coordinate origin difference。
- uniform alignment。
- texture / sampler binding。
- float / half-float render target availability。
- Web 平台 format fallback。

### 4.3 许可证

- ofxFlowTools 是 MIT License，可作为参考或迁移基础，但必须保留 attribution。
- PixelFlow 是 MIT License，可作为参考或迁移基础，但必须保留 attribution。
- 新 addon 需要包含 `LICENSES_THIRD_PARTY.md` 或 README 章节，明确说明：
  - ofxFlowTools 参考来源。
  - PixelFlow 参考来源。
  - shader / algorithm 来源。
  - 是否直接复用代码，还是概念重写。
  - 如果复制任何 shader 片段，必须保留原始版权说明与 license。

### 4.4 不允许的做法

- 不要依赖 openFrameworks。
- 不要依赖 Processing / Java。
- 不要引入 GPL 或不兼容许可证依赖。
- 不要只做一个 demo shader。
- 不要把 GUI 作为核心依赖。
- 不要把 `tcxImGui` 作为核心 addon 的硬依赖。GUI 可在 example 中作为 optional。
- 不要隐藏失败项或未完成项。
- 不要每帧重建 shader、FBO、texture。
- 不要每帧同步 GPU readback，除非 debug 显式启用。

---

## 5. 必须先完成的调研任务

在写核心代码前，先完成代码调研并生成迁移矩阵。必须输出：

```text
MIGRATION_REPORT.md
HD_NOTES.md
PORTABILITY_NOTES.md
KNOWN_LIMITATIONS.md
```

第一版迁移矩阵至少包括：

| 来源 | 原始模块 / 类 | 新模块 / 类 | 状态 | 备注 |
|---|---|---|---|---|
| ofxFlowTools master | `ftPingPongFbo` | `PingPongBuffer` | port / rewrite | GPU ping-pong buffer |
| ofxFlowTools master / HD | `ftFluidFlow` | `Fluid2D` | port / rewrite | fluid solver |
| ofxFlowTools master / HD | `ftOpticalFlow` | `OpticalFlow` | port / rewrite | frame-to-frame flow |
| ofxFlowTools master / HD | bridge classes | `BridgeFlow` hierarchy | port / rewrite | input-to-fluid |
| ofxFlowTools master / HD | visualization classes | visualizers | port / rewrite | debug + output |
| ofxFlowTools extensions | mouse / average / particles / splitvelocity | extension modules | staged | optional after core |
| PixelFlow | `DwFluid2D` | algorithm reference | reference | pressure / velocity / density pipeline |
| PixelFlow | `DwOpticalFlow` | algorithm reference | reference | optical flow reference |
| PixelFlow | filters | utility passes | reference | blur / difference / sobel / luminance / threshold |
| PixelFlow | flow-field particles / streamlines | extension reference | staged | particles / visualization |

必须检查：

```text
ofxFlowTools/src/core/
ofxFlowTools/src/core/bridge/
ofxFlowTools/src/core/fluid/
ofxFlowTools/src/core/opticalflow/
ofxFlowTools/src/core/shaders/
ofxFlowTools/src/core/visualization/
ofxFlowTools/src/extensions/
ofxFlowTools/example_core/
ofxFlowTools/example_extended/
ofxFlowTools/example_extended_average/
ofxFlowTools HD/example_simple/
PixelFlow/src/com/thomasdiewald/pixelflow/java/fluid/
PixelFlow/src/com/thomasdiewald/pixelflow/java/imageprocessing/filter/
PixelFlow/src/com/thomasdiewald/pixelflow/java/flowfieldparticles/
PixelFlow/examples/
TrussC/addons/tcxImGui/
TrussC/examples/graphics/
TrussC/docs/REFERENCE.md
```

---

## 6. 推荐目录结构

创建：

```text
addons/tcxFlowTools/
  addon.json
  CMakeLists.txt
  README.md
  LICENSES_THIRD_PARTY.md
  MIGRATION_REPORT.md
  HD_NOTES.md
  PORTABILITY_NOTES.md
  KNOWN_LIMITATIONS.md

  src/
    tcxFlowTools.h

    tcxFlow/
      Core/
        PingPongBuffer.h
        PingPongBuffer.cpp
        FlowPass.h
        FlowPass.cpp
        FullscreenPass.h
        FullscreenPass.cpp
        TextureUtils.h
        TextureUtils.cpp
        FlowTypes.h
        FlowSettings.h
        ShaderUtils.h
        ShaderUtils.cpp
        ResourceLabel.h

      Fluid/
        Fluid2D.h
        Fluid2D.cpp
        FluidSettings.h
        FluidBuffers.h
        FluidPasses.h
        FluidPasses.cpp

      OpticalFlow/
        OpticalFlow.h
        OpticalFlow.cpp
        OpticalFlowSettings.h

      Bridge/
        BridgeFlow.h
        BridgeFlow.cpp
        VelocityBridge.h
        VelocityBridge.cpp
        DensityBridge.h
        DensityBridge.cpp
        TemperatureBridge.h
        TemperatureBridge.cpp
        CombinedBridge.h
        CombinedBridge.cpp

      Visualization/
        FlowVisualizer.h
        FlowVisualizer.cpp
        DensityVisualizer.h
        DensityVisualizer.cpp
        VelocityFieldVisualizer.h
        VelocityFieldVisualizer.cpp
        PressureVisualizer.h
        PressureVisualizer.cpp
        TemperatureVisualizer.h
        TemperatureVisualizer.cpp

      Extensions/
        MouseFlow.h
        MouseFlow.cpp
        AverageFlow.h
        AverageFlow.cpp
        SplitVelocity.h
        SplitVelocity.cpp
        ParticleFlow.h
        ParticleFlow.cpp
        ParticleFlowSettings.h

  shaders/
    common/
    fluid/
    opticalflow/
    bridge/
    visualization/
    particles/

  examples/
    example-simple/
    example-optical-flow/
    example-camera-fluid/
    example-fluid-bridges/
    example-particles/
    example-hd/

  tests/
    CMakeLists.txt
    test_settings.cpp
    test_resize.cpp
    test_resource_lifetime.cpp
```

当前实现统一使用 `examples/example-*`，README 与阶段审计中必须同步说明。

---

## 7. 顶层 API 设计

### 7.1 顶层 include

用户应能这样使用：

```cpp
#include <TrussC.h>
#include <tcxFlowTools.h>
```

### 7.2 命名空间

建议使用：

```cpp
namespace tcx::flow
```

如果 TrussC addon 现有命名方式不同，可调整，但必须全项目一致。

### 7.3 最小用户代码目标

`example-simple` 的核心使用方式应该接近：

```cpp
#include <TrussC.h>
#include <tcxFlowTools.h>

using namespace tc;
using namespace tcx::flow;

class MyApp : public App {
public:
    Fluid2D fluid;

    void setup() override {
        FluidSettings settings;
        settings.solverIterations = 20;
        settings.enableVorticity = true;
        fluid.setup(getWindowWidth(), getWindowHeight(), settings);
    }

    void update() override {
        float dt = getLastFrameTime();
        fluid.update(dt);
    }

    void draw() override {
        clear(0.0f);
        fluid.drawDensity(0, 0, getWindowWidth(), getWindowHeight());
    }

    void mouseDragged(int x, int y, int button) override {
        Vec2 p(x, y);
        Vec2 v(x - getPreviousMouseX(), y - getPreviousMouseY());
        fluid.addVelocity(p, 24.0f, v * 10.0f);
        fluid.addDensity(p, 32.0f, Color(0.2f, 0.7f, 1.0f, 1.0f));
    }
};

int main() {
    WindowSettings settings;
    settings.setSize(1280, 720);
    settings.setTitle("tcxFlowTools example-simple");
    return runApp<MyApp>(settings);
}
```

上面是目标 API 风格，不要求逐字匹配。实际函数名以 TrussC API 能力与命名规范为准。

---

## 8. 核心类要求

### 8.1 `PingPongBuffer`

职责：

- 管理两组 GPU render target / texture。
- 支持 `allocate(width, height, format)`。
- 支持 `resize(width, height)`。
- 支持 `swap()`。
- 支持 `read()` / `write()` 或 `src()` / `dst()`。
- 支持 `clear()`。
- 支持 label / debug name。
- 支持 move construction / move assignment。
- 禁止 copy，避免 GPU resource 双重释放。
- 不暴露底层 OpenGL 句柄。

建议接口：

```cpp
class PingPongBuffer {
public:
    PingPongBuffer() = default;
    ~PingPongBuffer();

    PingPongBuffer(const PingPongBuffer&) = delete;
    PingPongBuffer& operator=(const PingPongBuffer&) = delete;
    PingPongBuffer(PingPongBuffer&&) noexcept;
    PingPongBuffer& operator=(PingPongBuffer&&) noexcept;

    void allocate(int width, int height, TextureFormat format, const char* label = nullptr);
    void resize(int width, int height);
    void clear(const Color& color = Color(0, 0, 0, 0));
    void swap();
    void release();

    bool isAllocated() const;
    int width() const;
    int height() const;
    TextureFormat format() const;

    Texture& read();
    Texture& write();
    const Texture& read() const;
    const Texture& write() const;

    RenderTarget& writeTarget();
};
```

类型名如 `Texture`、`RenderTarget`、`TextureFormat` 需要替换为 TrussC 当前实际 GPU 类型。

### 8.2 `FlowPass`

职责：

- 单个 fullscreen shader pass。
- 统一处理 shader、uniform、输入 texture、输出 render target。
- 为 fluid / optical flow / bridge / visualization / filters 共享。
- 支持 shader compile error 输出。
- 支持 named uniforms / typed uniform block。

建议接口：

```cpp
class FlowPass {
public:
    void setup(const std::string& shaderPath, const std::string& label = {});
    void setTexture(const std::string& name, const Texture& texture);
    template <typename T>
    void setUniform(const std::string& name, const T& value);
    void render(RenderTarget& target);
    void render(Texture& outputTextureOrFbo);
    bool isReady() const;
};
```

### 8.3 `FullscreenPass`

职责：

- 统一 fullscreen triangle / quad。
- 屏蔽坐标系与 UV 翻转差异。
- 所有 post-process pass 都通过它绘制。

注意：不要假设 `gl_VertexID` 在所有后端都可用；如果 TrussC / sokol shader 工具支持，则可使用 fullscreen triangle；否则使用一个内部 quad mesh。

### 8.4 `Fluid2D`

职责：

- 管理 velocity、density、temperature、pressure、divergence、curl、obstacle 等 buffer。
- 实现完整 fluid solver pipeline。
- 提供 draw / texture access API。
- 允许 external source 注入。
- 支持 resize。
- 支持 resolution scale。

建议接口：

```cpp
class Fluid2D {
public:
    void setup(int width, int height, const FluidSettings& settings = {});
    void resize(int width, int height);
    void update(float dt);
    void reset();
    void clear();

    void addVelocity(const Vec2& position, float radius, const Vec2& velocity);
    void addDensity(const Vec2& position, float radius, const Color& color);
    void addTemperature(const Vec2& position, float radius, float temperature);

    void applyVelocityTexture(const Texture& velocityTexture, float scale = 1.0f);
    void applyDensityTexture(const Texture& densityTexture, float scale = 1.0f);
    void applyTemperatureTexture(const Texture& temperatureTexture, float scale = 1.0f);

    void drawDensity(float x, float y, float w, float h) const;
    void drawVelocity(float x, float y, float w, float h) const;
    void drawPressure(float x, float y, float w, float h) const;
    void drawTemperature(float x, float y, float w, float h) const;

    const Texture& getVelocityTexture() const;
    const Texture& getDensityTexture() const;
    const Texture& getPressureTexture() const;
    const Texture& getTemperatureTexture() const;
    const Texture& getDivergenceTexture() const;

    FluidSettings& settings();
    const FluidSettings& settings() const;

    int simWidth() const;
    int simHeight() const;
    float resolutionScale() const;
};
```

### 8.5 `OpticalFlow`

职责：

- 接收当前帧 texture。
- 保存前一帧。
- 输出 flow texture。
- 支持 optional downsample / preprocess。
- 支持 debug draw。

建议接口：

```cpp
class OpticalFlow {
public:
    void setup(int width, int height, const OpticalFlowSettings& settings = {});
    void resize(int width, int height);
    void update(const Texture& inputTexture, float dt);
    void reset();

    const Texture& getFlowTexture() const;
    const Texture& getCurrentTexture() const;
    const Texture& getPreviousTexture() const;

    void drawFlow(float x, float y, float w, float h) const;
    void drawDebug(float x, float y, float w, float h) const;

    OpticalFlowSettings& settings();
    const OpticalFlowSettings& settings() const;
};
```

### 8.6 `BridgeFlow`

职责：

- 把外部 texture / mask / flow / brightness / silhouette / mouse input 转成 fluid 可消费的注入 texture。
- 提供统一更新接口。
- 派生类分别处理 velocity、density、temperature、combined。

建议接口：

```cpp
class BridgeFlow {
public:
    virtual ~BridgeFlow() = default;
    virtual void setup(int width, int height) = 0;
    virtual void resize(int width, int height) = 0;
    virtual void update(const Texture& input, float dt) = 0;
    virtual void applyTo(Fluid2D& fluid) = 0;
};
```

### 8.7 `ParticleFlow`

职责：

- GPU 或 fallback 粒子系统。
- 使用 velocity field 更新粒子位置。
- 支持 spawn / reset / lifetime / draw。
- 如果 TrussC 当前不支持 transform feedback / storage buffer / compute shader，则实现 texture-based particles 或 CPU fallback，并在 `KNOWN_LIMITATIONS.md` 中说明限制。

---

## 9. Settings 要求

### 9.1 `FluidSettings`

至少包含：

```cpp
struct FluidSettings {
    int solverIterations = 20;
    float timestep = 1.0f / 60.0f;

    float resolutionScale = 1.0f;

    float velocityDissipation = 0.99f;
    float densityDissipation = 0.995f;
    float temperatureDissipation = 0.99f;
    float pressureDissipation = 0.98f;

    float vorticity = 0.2f;
    float viscosity = 0.0f;
    float buoyancy = 0.0f;
    float densityWeight = 0.0f;

    float inputVelocityScale = 1.0f;
    float inputDensityScale = 1.0f;
    float inputTemperatureScale = 1.0f;

    bool enableVorticity = true;
    bool enableTemperature = true;
    bool enableObstacles = false;
    bool enableBuoyancy = false;
    bool autoClearForces = true;
};
```

### 9.2 `OpticalFlowSettings`

至少包含：

```cpp
struct OpticalFlowSettings {
    float strength = 1.0f;
    float offset = 1.0f;
    float lambda = 0.01f;
    float threshold = 0.0f;
    float decay = 0.95f;
    float blurRadius = 2.0f;
    float flowScale = 1.0f;
    float temporalSmoothing = 0.5f;

    bool normalizeInput = true;
    bool mirrorX = false;
    bool mirrorY = false;
    bool useLuminance = true;
};
```

### 9.3 `BridgeSettings`

至少包含：

```cpp
struct BridgeSettings {
    float threshold = 0.0f;
    float gain = 1.0f;
    float decay = 0.95f;
    float blurRadius = 0.0f;
    float velocityScale = 1.0f;
    float densityScale = 1.0f;
    float temperatureScale = 1.0f;

    bool invert = false;
    bool useAlphaAsMask = false;
    bool mirrorX = false;
    bool mirrorY = false;
};
```

### 9.4 `ParticleFlowSettings`

至少包含：

```cpp
struct ParticleFlowSettings {
    int particleCount = 65536;
    float lifetime = 5.0f;
    float velocityScale = 1.0f;
    float damping = 0.995f;
    float spawnRadius = 1.0f;
    bool respawn = true;
    bool useGpuParticles = true;
};
```

---

## 10. Fluid pipeline 要求

`Fluid2D::update(dt)` 至少执行以下阶段：

1. Accumulate external inputs。
2. Velocity advection。
3. Density advection。
4. Temperature advection。
5. Buoyancy，可配置。
6. Vorticity confinement，可配置。
7. Divergence pass。
8. Pressure clear / pressure dissipation。
9. Pressure solve，Jacobi iterations。
10. Gradient subtract / projection。
11. Boundary handling。
12. Optional obstacle handling。
13. Auto-clear transient source buffers。

伪代码：

```cpp
void Fluid2D::update(float dt) {
    float step = settings_.timestep > 0.0f ? settings_.timestep : dt;

    advectVelocity(step);
    advectDensity(step);

    if (settings_.enableTemperature) {
        advectTemperature(step);
    }

    if (settings_.enableBuoyancy) {
        applyBuoyancy(step);
    }

    if (settings_.enableVorticity) {
        computeCurl();
        applyVorticity(step);
    }

    computeDivergence();
    clearPressure();

    for (int i = 0; i < settings_.solverIterations; ++i) {
        solvePressureJacobi();
    }

    subtractGradient();

    if (settings_.autoClearForces) {
        clearTransientInputs();
    }
}
```

验收时必须证明：

- 鼠标注入 velocity / density 后 fluid 可见。
- output 不全黑、不全白、不 NaN。
- resize 后不崩溃。
- solverIterations 可调。
- vorticity / dissipation 参数有效。

---

## 11. Optical flow 要求

`OpticalFlow` 至少支持：

- current frame。
- previous frame。
- luminance conversion。
- blur / smoothing。
- frame difference。
- local gradient / offset sampling。
- threshold。
- strength。
- decay。
- temporal smoothing。
- flow visualization。
- output RG 或 RGBA flow texture。
- optional mirror X / mirror Y。

典型 pipeline：

1. Copy / normalize input。
2. Convert to luminance or selected channel。
3. Blur current and previous。
4. Estimate optical flow。
5. Threshold / scale。
6. Temporal smoothing with previous flow。
7. Store flow texture。
8. Swap current / previous frame buffers。

必须实现一个能被 fluid 使用的输出：

```cpp
fluid.applyVelocityTexture(opticalFlow.getFlowTexture(), flowToFluidScale);
```

`example-optical-flow` 必须用 moving procedural texture 或 webcam 测试 optical flow。如果 webcam 不可用，必须自动 fallback 到 procedural moving shapes。

---

## 12. Bridge 模块要求

Bridge 模块目标是把视觉输入变成 fluid source。

### 12.1 `VelocityBridge`

- 输入：flow texture 或 optical flow output。
- 输出：velocity injection。
- 参数：scale、threshold、blur、mask、decay。
- 应支持直接 `applyTo(Fluid2D&)`。

### 12.2 `DensityBridge`

- 输入：camera / video / image texture。
- 输出：density injection。
- 可按 brightness、alpha、color、mask 注入。
- 参数：threshold、gain、color multiplier、decay。

### 12.3 `TemperatureBridge`

- 输入：brightness / mask / custom texture。
- 输出：temperature injection。
- 可结合 buoyancy。

### 12.4 `CombinedBridge`

- 同时处理 velocity、density、temperature。
- 适合 camera-fluid 示例。
- 允许单独开关各分量。

---

## 13. Visualization 要求

必须实现：

1. `DensityVisualizer`
   - 显示 density texture。
   - 支持 exposure / gamma / alpha。

2. `VelocityFieldVisualizer`
   - velocity to color。
   - vector arrows / grid arrows。
   - scale、stride、opacity 可调。

3. `PressureVisualizer`
   - scalar pressure 显示。
   - 支持 range / auto range optional。

4. `TemperatureVisualizer`
   - scalar temperature 显示。
   - 支持 color ramp optional。

5. `FlowVisualizer`
   - generic texture visualizer。
   - 能显示 RG flow、RGBA texture、single-channel scalar。

所有 visualizer 都应避免 readback，优先 shader draw。

---

## 14. HD 分支专项要求

必须专门检查 ofxFlowTools 的 `HD` 分支，并完成以下工作：

1. 对比 `master` 与 `HD` 的：
   - shader 差异。
   - buffer format 差异。
   - resolution handling。
   - examples。
   - API / 参数变化。
   - resize behavior。

2. 抽取 HD 分支中真正有价值的改动。

3. 在新 addon 中实现 **resolution scale**：
   - input resolution。
   - simulation resolution。
   - display resolution。
   - 可设置 `simScale = 1.0 / 0.5 / 0.25`。

4. 支持高分辨率 camera / video / texture 输入，但允许低分辨率 fluid simulation。

5. 支持 resize 时自动重建 GPU buffer。

6. 提供 `example-hd`，演示 1080p 或更高输入下的 half-res / quarter-res simulation。

7. 输出 `HD_NOTES.md`，说明：
   - HD 分支中哪些内容被采用。
   - 哪些内容被放弃。
   - 放弃原因。
   - 新 addon 中 resolution scale 的实现细节。
   - 不同平台的格式 fallback。

验收时不能只说“已参考 HD 分支”。必须能看到具体 diff / table / 文件映射。

---

## 15. Shader 迁移要求

### 15.1 迁移原则

将 ofxFlowTools / PixelFlow 中的 GLSL 思路迁移为 TrussC / sokol 兼容 shader：

- 使用统一 fullscreen triangle 或 fullscreen quad。
- 使用明确 uniform block。
- 使用明确 texture / sampler binding。
- 不使用 `gl_FragColor`。
- 不使用 openFrameworks shader include。
- 不使用 Processing uniform 命名约定作为硬编码前提。
- 不依赖 OpenGL-only extension。
- shader 文件放在 addon 内部，并可被 CMake 或 TrussC shader 工具处理。
- shader 编译失败必须输出清晰错误信息。

### 15.2 Shader pass 清单

至少实现：

```text
shaders/common/
  fullscreen.vert 或 fullscreen shader 片段
  copy
  clear
  multiply
  threshold
  luminance
  difference
  blur_horizontal
  blur_vertical

shaders/fluid/
  advect
  divergence
  jacobi_pressure
  gradient_subtract
  splat_velocity
  splat_density
  splat_temperature
  curl
  vorticity
  buoyancy
  boundary
  obstacle_optional

shaders/opticalflow/
  preprocess
  optical_flow
  temporal_smooth
  flow_visualize

shaders/bridge/
  velocity_bridge
  density_bridge
  temperature_bridge
  combined_bridge

shaders/visualization/
  scalar_visualize
  velocity_color_visualize
  vector_field_visualize
  density_visualize

shaders/particles/
  update_particles
  draw_particles
```

### 15.3 Cross-backend 注意事项

必须处理：

- UV 翻转。
- texture origin。
- Metal / D3D / Web uniform alignment。
- half-float / float texture format fallback。
- linear / nearest sampling 设置。
- wrap / clamp 设置。
- premultiplied alpha 与非 premultiplied alpha。
- Web 平台可能不支持某些 renderable texture format。
- shader codegen 或 slang 工具的路径问题。

建议提供一个统一的 shader common block，例如：

```cpp
struct FlowCommonUniforms {
    Vec2 resolution;
    Vec2 invResolution;
    float dt;
    float time;
    float flipY;
};
```

实际 uniform alignment 必须以 sokol / TrussC 规范为准。

---

## 16. Examples 要求

### 16.1 `example-simple`

目标：最小可运行 fluid 示例。

必须包含：

- 鼠标拖拽注入 velocity。
- 鼠标位置注入 density。
- 显示 density。
- 无摄像头依赖。
- 可作为 smoke test。
- 键盘快捷键：
  - `r` reset。
  - `v` toggle velocity visualization。
  - `d` density visualization。
  - `p` pressure visualization。

### 16.2 `example-optical-flow`

目标：展示 optical flow。

必须包含：

- 使用 webcam 或 procedural moving texture。
- webcam 不可用时自动 fallback。
- 显示原始输入。
- 显示 flow visualization。
- 显示 flow texture 驱动 fluid 的效果。
- 可调 flow strength / threshold / blur / decay。

### 16.3 `example-camera-fluid`

目标：接近 ofxFlowTools 原始用途。

必须包含：

- camera / video input。
- optical flow 驱动 fluid velocity。
- brightness / silhouette / motion 注入 density。
- 支持 GUI 或键盘调参。
- GUI 只能 optional，核心 example 必须能无 GUI 运行。
- 摄像头不可用时使用 procedural fallback。

### 16.4 `example-fluid-bridges`

目标：测试 bridge 模块。

必须包含：

- 单独演示 velocity bridge。
- 单独演示 density bridge。
- 单独演示 temperature bridge。
- 演示 combined bridge。
- 可以通过键盘切换模式。

### 16.5 `example-particles`

目标：测试 particle flow。

必须包含：

- 粒子受 velocity field 影响。
- 支持 reset。
- 支持粒子数量配置。
- 支持 velocity scale / damping / lifetime。
- 如果 GPU particle 受限，说明 fallback。

### 16.6 `example-hd`

目标：测试 HD 输入与低分辨率 simulation。

必须包含：

- 输入 1080p 或 procedural high-res texture。
- simulation resolution 可切换 1x / 0.5x / 0.25x。
- 显示性能信息。
- 显示最终 upsample output。
- resize 时不崩溃。
- 明确展示 input resolution、simulation resolution、display resolution。

---

## 17. 文档要求

### 17.1 `README.md`

至少包含：

1. addon 简介。
2. 安装方式。
3. CMake / TrussC 集成方式。
4. `addons.make` 或 `trusscli add` 使用方式，如果当前 TrussC 支持。
5. 最小代码示例。
6. 所有 examples 说明。
7. API 快速说明。
8. 参数说明。
9. 跨平台注意事项。
10. HD 分支迁移说明。
11. PixelFlow / ofxFlowTools attribution。
12. 当前限制。
13. 后续 roadmap。
14. Troubleshooting。

### 17.2 `LICENSES_THIRD_PARTY.md`

至少包含：

```text
ofxFlowTools
- Repository: https://github.com/moostrik/ofxFlowTools
- License: MIT
- Used as: algorithm/API/shader reference, partial rewritten concepts
- Notes: cite actual files if shader snippets or code are reused

PixelFlow
- Repository: https://github.com/diwi/PixelFlow
- Author: Thomas Diewald
- License: MIT
- Used as: algorithm/filter/fluid/optical-flow/particle reference
- Notes: cite actual files if shader snippets or code are reused

Other references
- Mark J. Harris stable fluids material where relevant
- Jos Stam stable fluids material where relevant
- Any shader snippets actually reused
```

注意：具体作者字段以仓库实际 license / README / source header 为准，不要凭空写死。

### 17.3 `MIGRATION_REPORT.md`

每个被参考、迁移、重写或跳过的模块都要记录：

```text
Original source file:
Original branch:
New file:
Status: copied / ported / rewritten / referenced / skipped
Reason:
License notes:
Implementation notes:
Tests / examples:
```

### 17.4 `HD_NOTES.md`

必须包含：

- master vs HD 差异表。
- HD 分支新增文件 / 目录。
- HD 分支 shader 差异。
- HD 分支 example 差异。
- 对 tcxFlowTools 的采用项。
- 未采用项与原因。
- resolution scale 的实现方式。

### 17.5 `PORTABILITY_NOTES.md`

必须包含：

- macOS / Metal。
- Windows / D3D 或 GL fallback。
- Linux。
- Web / Emscripten。
- texture format fallback。
- shader limitation。
- webcam availability fallback。

### 17.6 `KNOWN_LIMITATIONS.md`

必须列出：

- 尚未实现的模块。
- 受 TrussC / sokol 限制的功能。
- Web 平台降级项。
- particle fallback 情况。
- 未测试平台。

---

## 18. 测试与验收标准

### 18.1 构建验收

至少完成当前平台构建，例如 macOS：

```bash
cmake --preset macos
cmake --build build-macos --parallel
```

并根据当前 TrussC repo 支持情况尽量完成：

```bash
cmake --preset windows
cmake --preset linux
cmake --preset web
```

如果某个平台因 TrussC 当前限制、依赖缺失或 CI 环境缺失无法测试，必须在 `PORTABILITY_NOTES.md` 中明确记录。

### 18.2 功能验收

必须满足：

- addon 可以被 TrussC app include。
- `example-simple` 可运行。
- `example-optical-flow` 可运行。
- `example-camera-fluid` 或 fallback 版本可运行。
- resize 不崩溃。
- shader 编译失败时有明确错误。
- fluid output 不全黑、不全白、不 NaN。
- optical flow 在 moving input 下能产生非零 flow。
- optical flow 可以驱动 fluid velocity。
- density / velocity / pressure / temperature buffer 可被 visualizer 绘制。
- HD example 可切换 simulation scale。
- bridge 模块可以把外部 texture 注入 fluid。

### 18.3 质量验收

必须满足：

- 没有 openFrameworks 依赖。
- 没有 Processing / Java 依赖。
- 没有直接 OpenGL-only public path。
- GPU resource 使用 RAII 管理。
- 类名、文件名、namespace 一致。
- settings 可复制。
- settings 有默认值。
- examples 不依赖绝对路径。
- README 可让新用户在 10 分钟内跑起 `example-simple`。
- 所有失败项写入 `KNOWN_LIMITATIONS.md`。

### 18.4 性能目标

非硬性但应努力达到：

- 720p input + 0.5x simulation 在普通独显上接近 60 FPS。
- 1080p input + 0.25x simulation 在普通独显上可交互。
- Web 平台允许降级 resolution / iterations。
- 不每帧重建 GPU resource。
- 不默认做 GPU readback。

---

## 19. 实施阶段

### Phase 1：仓库调研与 scaffolding

任务：

1. 检查 TrussC 当前 addon 规范。
2. 检查现有 `addons/tcx*` 的 `CMakeLists.txt`、`addon.json`、example 结构。
3. 创建 `addons/tcxFlowTools/`。
4. 创建 README 初稿和第三方许可证说明。
5. 创建空 addon，可被 include，可被 CMake 构建。

交付物：

```text
addons/tcxFlowTools/addon.json
addons/tcxFlowTools/CMakeLists.txt
addons/tcxFlowTools/README.md
addons/tcxFlowTools/LICENSES_THIRD_PARTY.md
addons/tcxFlowTools/src/tcxFlowTools.h
```

验收：

- TrussC 可以识别 addon。
- 一个空 example 能 include `<tcxFlowTools.h>` 并构建。

### Phase 2：GPU 基础设施

任务：

1. 实现 `PingPongBuffer`。
2. 实现 `FlowPass`。
3. 实现 fullscreen draw helper。
4. 实现 texture format selection / fallback。
5. 实现 resize / clear / swap。
6. 添加基础 shader：copy、clear、multiply、threshold、luminance、difference、blur。

交付物：

```text
src/tcxFlow/Core/PingPongBuffer.*
src/tcxFlow/Core/FlowPass.*
src/tcxFlow/Core/FullscreenPass.*
src/tcxFlow/Core/TextureUtils.*
shaders/common/*
```

验收：

- `example-simple` 的临时版本可以显示 copy / clear pass。
- resize 不崩溃。

### Phase 3：Fluid2D

任务：

1. 实现 velocity、density、temperature、pressure、divergence、curl buffers。
2. 实现 advect。
3. 实现 splat。
4. 实现 divergence。
5. 实现 Jacobi pressure solve。
6. 实现 gradient subtract。
7. 实现 vorticity confinement。
8. 实现 buoyancy。
9. 实现 density / velocity / pressure / temperature draw。

交付物：

```text
src/tcxFlow/Fluid/Fluid2D.*
src/tcxFlow/Fluid/FluidSettings.h
src/tcxFlow/Fluid/FluidBuffers.h
shaders/fluid/*
examples/example-simple/
```

验收：

- `example-simple` 完成。
- 鼠标拖拽可注入 velocity / density。
- 能切换 density / velocity / pressure debug view。

### Phase 4：OpticalFlow

任务：

1. 实现 previous / current frame 管理。
2. 实现 luminance / blur / difference。
3. 实现 optical flow shader。
4. 实现 temporal smoothing。
5. 实现 flow visualization。
6. 将 optical flow 输出连接到 Fluid2D velocity。

交付物：

```text
src/tcxFlow/OpticalFlow/OpticalFlow.*
src/tcxFlow/OpticalFlow/OpticalFlowSettings.h
shaders/opticalflow/*
examples/example-optical-flow/
```

验收：

- procedural moving texture 下可产生非零 flow。
- flow 可视化正确。
- flow 可驱动 Fluid2D。

### Phase 5：Bridge 模块

任务：

1. 实现 `BridgeFlow` 基类。
2. 实现 `VelocityBridge`。
3. 实现 `DensityBridge`。
4. 实现 `TemperatureBridge`。
5. 实现 `CombinedBridge`。
6. 加入 mask / threshold / gain / decay / blur 参数。

交付物：

```text
src/tcxFlow/Bridge/*
shaders/bridge/*
examples/example-fluid-bridges/
examples/example-camera-fluid/
```

验收：

- bridge 模块能把外部 texture 注入 fluid。
- camera-fluid example 可运行或 fallback 可运行。

### Phase 6：Visualization 与 debug

任务：

1. 实现 scalar visualizer。
2. 实现 velocity color visualizer。
3. 实现 vector field visualizer。
4. 实现 pressure / temperature visualizer。
5. 增加 debug draw API。

交付物：

```text
src/tcxFlow/Visualization/*
shaders/visualization/*
```

验收：

- fluid / optical flow / bridge examples 中都能打开 debug view。

### Phase 7：Extensions

任务：

1. 实现 `MouseFlow`。
2. 实现 `AverageFlow`。
3. 实现 `SplitVelocity`。
4. 实现 `ParticleFlow`。
5. optional：`TuringPattern`，仅在核心稳定后实现。

交付物：

```text
src/tcxFlow/Extensions/*
shaders/particles/*
examples/example-particles/
```

验收：

- 粒子受 velocity field 影响。
- reset / lifetime / velocity scale 有效。

### Phase 8：HD pipeline

任务：

1. 完成 master vs HD 差异记录。
2. 实现 resolution scale。
3. 实现 high-res input + low-res simulation。
4. 实现 upsample / composite。
5. 完成 `example-hd`。

交付物：

```text
HD_NOTES.md
examples/example-hd/
```

验收：

- 可切换 1.0 / 0.5 / 0.25 simulation scale。
- 能显示 input resolution、simulation resolution、display resolution。

### Phase 9：测试、文档、清理

任务：

1. 添加基础 tests。
2. 清理 API。
3. 清理 shader 命名。
4. 检查所有 examples。
5. 补全 README。
6. 补全 attribution。
7. 输出最终迁移报告。

交付物：

```text
tests/
MIGRATION_REPORT.md
PORTABILITY_NOTES.md
KNOWN_LIMITATIONS.md
README.md 完整版
```

验收：

- 构建结果写入文档。
- 已测试平台写入文档。
- examples 运行状态写入文档。
- 未完成项写入文档。

---

## 20. Codex 执行规则

1. 每完成一个 phase，先尝试编译。
2. 不要一次性生成大量未经测试的代码。
3. 遇到 TrussC API 不确定时，优先搜索当前仓库中的相似用法。
4. 以现有 TrussC examples 为准，不要假设 openFrameworks API 存在。
5. shader 迁移后必须检查 uniform 名称、texture binding、坐标系。
6. 对每个被重写的 ofxFlowTools / PixelFlow 模块，在 `MIGRATION_REPORT.md` 中记录：
   - 原始参考文件。
   - 新文件。
   - 是否直接移植、重写、跳过。
   - 跳过原因。
7. 不要隐藏失败项。任何无法完成的模块都必须写入 `KNOWN_LIMITATIONS.md`。
8. 最终输出前必须给出：
   - 构建结果。
   - 已测试平台。
   - examples 运行状态。
   - 未完成项。
   - 下一步建议。
9. 不要引入新的重型依赖，除非 TrussC 仓库已有并允许。
10. 所有 GPU resource 生命周期必须清晰，避免 leak、double free、dangling texture reference。

---

## 21. 建议的第一轮文件内容

### 21.1 `addon.json`

```json
{
  "description": "GPU fluid simulation, optical flow, bridge flows, visualization and particles for TrussC",
  "author": "TrussC community / generated implementation",
  "license": "MIT",
  "keywords": [
    "gpu",
    "fluid",
    "optical-flow",
    "flow-field",
    "shader",
    "visuals",
    "creative-coding"
  ]
}
```

### 21.2 `src/tcxFlowTools.h`

```cpp
#pragma once

#include "tcxFlow/Core/FlowTypes.h"
#include "tcxFlow/Core/PingPongBuffer.h"
#include "tcxFlow/Core/FlowPass.h"
#include "tcxFlow/Fluid/Fluid2D.h"
#include "tcxFlow/Fluid/FluidSettings.h"
#include "tcxFlow/OpticalFlow/OpticalFlow.h"
#include "tcxFlow/OpticalFlow/OpticalFlowSettings.h"
#include "tcxFlow/Bridge/BridgeFlow.h"
#include "tcxFlow/Bridge/VelocityBridge.h"
#include "tcxFlow/Bridge/DensityBridge.h"
#include "tcxFlow/Bridge/TemperatureBridge.h"
#include "tcxFlow/Bridge/CombinedBridge.h"
#include "tcxFlow/Visualization/FlowVisualizer.h"
#include "tcxFlow/Extensions/MouseFlow.h"
#include "tcxFlow/Extensions/ParticleFlow.h"
```

### 21.3 `CMakeLists.txt`

实际 CMake 必须检查 TrussC 当前约定后编写。可以从下面骨架开始：

```cmake
cmake_minimum_required(VERSION 3.20)

message(STATUS "[tcxFlowTools] Loading addon...")

file(GLOB_RECURSE TCX_FLOWTOOLS_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

file(GLOB_RECURSE TCX_FLOWTOOLS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

add_library(tcxFlowTools STATIC
    ${TCX_FLOWTOOLS_HEADERS}
    ${TCX_FLOWTOOLS_SOURCES}
)

target_include_directories(tcxFlowTools PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(tcxFlowTools PUBLIC TrussC)

# Make shader directory discoverable. Adjust to TrussC resource conventions.
target_compile_definitions(tcxFlowTools PUBLIC
    TCX_FLOWTOOLS_SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/shaders"
)

message(STATUS "[tcxFlowTools] Addon loaded")
```

---

## 22. 最终交付清单

最终应交付：

```text
addons/tcxFlowTools/
  addon.json
  CMakeLists.txt
  README.md
  LICENSES_THIRD_PARTY.md
  MIGRATION_REPORT.md
  HD_NOTES.md
  PORTABILITY_NOTES.md
  KNOWN_LIMITATIONS.md

  src/
    tcxFlowTools.h
    tcxFlow/Core/*
    tcxFlow/Fluid/*
    tcxFlow/OpticalFlow/*
    tcxFlow/Bridge/*
    tcxFlow/Visualization/*
    tcxFlow/Extensions/*

  shaders/
    common/*
    fluid/*
    opticalflow/*
    bridge/*
    visualization/*
    particles/*

  examples/
    example-simple/
    example-optical-flow/
    example-camera-fluid/
    example-fluid-bridges/
    example-particles/
    example-hd/

  tests/
```

核心验收目标：

```text
1. TrussC 可识别并构建 tcxFlowTools addon。
2. example-simple 能运行并显示 fluid。
3. optical flow 能从连续帧产生 motion texture。
4. optical flow 可以驱动 Fluid2D velocity。
5. bridge 模块可以把外部 texture 注入 fluid。
6. HD example 支持输入分辨率和 simulation 分辨率分离。
7. addon 无 openFrameworks / Processing / Java 依赖。
8. 代码跨平台，不绑定 OpenGL-only 路径。
9. README、HD_NOTES、MIGRATION_REPORT、PORTABILITY_NOTES、KNOWN_LIMITATIONS 完整。
```

---

## 23. 优先级总结

```text
P0: addon scaffolding + CMake + include 成功
P1: PingPongBuffer + FlowPass + shader pipeline
P2: Fluid2D 可运行
P3: OpticalFlow 可运行
P4: OpticalFlow 驱动 Fluid2D
P5: BridgeFlow 模块
P6: HD resolution scale
P7: Visualization / debug
P8: Particles / Average / SplitVelocity
P9: Turing patterns / advanced filters
```

判断标准：先做出稳定、可运行、跨平台的核心，再逐步补全高级功能。不得为了追求粒子或复杂后处理而牺牲 fluid / optical flow 的稳定性。

---

## 24. 给 Codex 的最终指令

请按照本文档执行，优先完成可构建、可运行、跨平台的核心。每个阶段都需要：

1. 阅读当前 TrussC 代码确认 API。
2. 修改或新增必要文件。
3. 尝试构建。
4. 修复编译错误。
5. 更新文档记录当前状态。
6. 明确列出未完成项。

最终输出必须包含：

```text
- 修改文件列表
- 构建命令与结果
- 已测试平台
- 可运行 examples
- 未完成项
- 迁移报告摘要
- 下一步建议
```
