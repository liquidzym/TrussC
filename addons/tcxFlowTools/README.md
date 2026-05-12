# tcxFlowTools

`tcxFlowTools` is a TrussC addon for fluid-style creative coding workflows. The fluid solver defaults to a sokol/TrussC GPU ping-pong graph; the CPU path remains only as a robustness fallback when no graphics context or GPU path is available.

The target is a sokol-compatible GPU rewrite of concepts from ofxFlowTools and PixelFlow: 2D fluid simulation, optical flow, bridges, visualizers, particles, and HD input pipelines.

## Status

Implemented now:

- TrussC addon metadata and CMake integration.
- Public include: `#include <tcxFlowTools.h>`.
- Namespace: `tcx::flow`.
- Settings structs for fluid, optical flow, bridge, and particles.
- `Fluid2D` GPU solver with resize, density/velocity/temperature injection, advection, optional vorticity and buoyancy, divergence, Jacobi pressure solve, projection, and GPU debug visualizers.
- CPU fallback for `Fluid2D` when `sg_isvalid()` is false, headless mode is active, or `FluidSettings::useGpu` is disabled.
- GPU obstacle mask support through `Fluid2D::addObstacle()` / `clearObstacles()`, with CPU fallback handling for no-GPU contexts.
- `applyVelocityField()` uploads external CPU vector fields as a dynamic RGBA32F texture and blends them into the GPU velocity buffer.
- `OpticalFlow::update(const tc::Texture&, float)` runs a GPU current/previous luminance optical-flow graph for camera/video/texture inputs and exposes `getFlowTexture()`, `getCurrentTexture()`, and `getPreviousTexture()`.
- `example-camera-fluid` connects `tc::VideoGrabber::getTexture()` directly to GPU optical flow, then injects the GPU flow texture into `Fluid2D`.
- Bridge class hierarchy: `BridgeFlow`, `VelocityBridge`, `DensityBridge`, `TemperatureBridge`, `CombinedBridge`.
- Visualization helpers and mouse/particle extension scaffolding.
- `ParticleFlow` defaults to GPU state textures, GPU update pass, and GPU particle drawing; CPU particles remain only as fallback.
- `example-simple` with mouse injection and density/velocity/pressure/temperature view switching.
- `example-core-pingpong`, `example-optical-flow`, `example-fluid-bridges`, `example-camera-fluid`, `example-particles`, and `example-hd`.
- Basic tests for settings, resize, density injection, and procedural optical-flow state.

Still limited:

- LIC/streamlines, full split-velocity shader parity, advanced bridge texture outputs, and PixelFlow-style particle variants remain pending.
- CPU tests intentionally exercise the fallback path because they run without an app graphics context.

## Install

Add the addon name to an app's `addons.make`:

```text
tcxFlowTools
```

Then include it:

```cpp
#include <TrussC.h>
#include <tcxFlowTools.h>
```

## Minimal Use

```cpp
tcx::flow::Fluid2D fluid;

void setup() {
    tcx::flow::FluidSettings settings;
    settings.resolutionScale = 0.5f;
    fluid.setup(tc::getWindowWidth(), tc::getWindowHeight(), settings);
}

void update() {
    fluid.update((float)tc::getDeltaTime());
}

void draw() {
    tc::clear(0.04f);
    fluid.drawDensity(0, 0, tc::getWindowWidth(), tc::getWindowHeight());
}

void mouseDragged(tc::Vec2 pos, int button) {
    fluid.addVelocity(pos, 24.0f, tc::Vec2(20.0f, 0.0f));
    fluid.addDensity(pos, 32.0f, tc::Color(0.2f, 0.7f, 1.0f, 1.0f));
}
```

## Examples

All example programs live under `addons/tcxFlowTools/examples/`.

`example-simple` is the current GPU fluid smoke test. It defaults to a half-resolution simulation, reference-scaled advection timestep, small continuous mouse impulses, and GPU density output.

Controls:

- Drag: inject velocity and density.
- `d`: density view.
- `v`: velocity debug view.
- `p`: pressure debug view.
- `t`: temperature debug view.
- `r`: reset.

Build:

```bash
cd addons/tcxFlowTools/examples/example-simple
cmake -S . -B build-macos
cmake --build build-macos --parallel
```

`example-core-pingpong` is the Phase 2 resource-lifetime smoke test for `PingPongBuffer`:

```bash
cd addons/tcxFlowTools/examples/example-core-pingpong
cmake -S . -B build-macos
cmake --build build-macos --parallel
```

Use `s` to swap read/write targets, `r` to resize, and `c` to repaint. The bottom row shows the generated common `FlowPass` copy and clear passes.

Phase 2 also includes generated common fullscreen shader passes:

- Source: `shaders/common/common.glsl`.
- Generated header: `shaders/common/common.glsl.h`.
- Passes: copy, clear, multiply, threshold, luminance, difference, blur horizontal, blur vertical.
- CMake compiles addon shader sources with sokol-shdc before building `tcxFlowTools`.

Phase 3 adds the fluid GPU solver path:

- Resource bundle: `src/tcxFlow/Fluid/FluidBuffers.h`.
- Source: `shaders/fluid/fluid.glsl`.
- Generated header: `shaders/fluid/fluid.glsl.h`.
- Passes: advect, splat, add velocity texture, divergence, Jacobi pressure, gradient subtract, vorticity curl, vorticity force, buoyancy.
- Obstacles: `addObstacle()` writes a GPU mask and clears velocity/density/temperature inside the mask; CPU fallback mirrors the same API.
- `Fluid2D::update()` uses the GPU graph by default and falls back to CPU only when no valid graphics context exists or GPU use is disabled.

Phase 4 adds optical-flow frame tracking and shader assets:

- Procedural moving luminance frame for tests/fallback, plus GPU texture input with previous/current luminance FBOs, gradient/time-difference flow estimate, temporal smoothing.
- Source: `shaders/opticalflow/opticalflow.glsl`.
- Generated header: `shaders/opticalflow/opticalflow.glsl.h`.
- Passes: luminance, difference, gradient, optical flow, temporal smooth, visualize.
- `example-optical-flow` shows the resulting flow driving `Fluid2D`.

Later phase shader assets are also generated by the addon build:

- Bridge: `shaders/bridge/bridge.glsl` with luminance mask, velocity, density, and temperature bridge passes.
- Visualization: `shaders/visualization/visualization.glsl` with scalar, velocity color, pressure, and temperature passes.
- Particles: `shaders/particles/particles.glsl` with spawn, update, and render helper passes.
- Extension APIs: `MouseFlow`, `AverageFlow`, `SplitVelocity`, and `ParticleFlow`.

Additional examples:

- `example-optical-flow`: procedural optical flow drives GPU `Fluid2D` through a velocity texture bridge.
- `example-fluid-bridges`: switches velocity, density, temperature, and combined bridges.
- `example-camera-fluid`: real camera texture drives GPU optical flow and GPU `Fluid2D`; procedural input is fallback only.
- `example-particles`: GPU particle state/update/draw over GPU fluid; CPU particles are fallback only.
- `example-hd`: GPU fluid with 1x / 0.5x / 0.25x simulation scale.

## Notes

This addon currently avoids TrussC core API changes. If future GPU pass work requires core additions, those changes should carry a dated comment explaining the addon requirement and cross-platform constraints.

See `MIGRATION_REPORT.md`, `REFERENCE_GAPS.md`, `HD_NOTES.md`, `PORTABILITY_NOTES.md`, and `KNOWN_LIMITATIONS.md` for current migration status.

## Verification Log

2026-05-09 on macOS / AppleClang 21:

```bash
cmake -S addons/tcxFlowTools/tests -B addons/tcxFlowTools/tests/build-macos
cmake --build addons/tcxFlowTools/tests/build-macos --parallel
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings

cmake -S addons/tcxFlowTools/examples/example-simple -B addons/tcxFlowTools/examples/example-simple/build-macos
cmake --build addons/tcxFlowTools/examples/example-simple/build-macos --parallel
```

Result:

- `tcxFlowTools_settings` built and passed.
- `example-simple` built successfully.
- Build emitted existing TrussC/stb/AVFoundation deprecation warnings; no `tcxFlowTools` compile errors were observed.

2026-05-10 on macOS / AppleClang 21:

```bash
cmake -S addons/tcxFlowTools/tests -B addons/tcxFlowTools/tests/build-macos
cmake --build addons/tcxFlowTools/tests/build-macos --parallel
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts

cmake -S addons/tcxFlowTools/examples/example-core-pingpong -B addons/tcxFlowTools/examples/example-core-pingpong/build-macos
cmake --build addons/tcxFlowTools/examples/example-core-pingpong/build-macos --parallel
cmake --build addons/tcxFlowTools/examples/example-simple/build-macos --parallel
```

Result:

- Settings and core-contract tests built and passed.
- `example-core-pingpong` built successfully.
- `example-simple` regression build passed.
- Phase 3 fallback solver test passed with finite density/velocity/pressure/temperature energy; app examples now use the GPU solver by default.
- `example-optical-flow`, `example-fluid-bridges`, `example-camera-fluid`, `example-particles`, and `example-hd` configured and built successfully.

Visual audit note:

- The first `example-simple` smoke run showed blocky output because the density renderer drew one enlarged rectangle per simulation cell while the app used a 0.25 simulation scale.
- The 2026-05-10 correction changed density output to GPU FBO drawing when the GPU solver is active, raised `example-simple` to 0.5 simulation scale, and tuned mouse injection/diffusion/vorticity.
- A follow-up visual audit still showed weak fluid character. Comparing against ofxFlowTools and PixelFlow found the core causes: pressure projection sign mismatch, too-small advection timestep, oversized mouse splats, and vorticity direction/scale mismatch.
- The follow-up correction aligns divergence/Jacobi projection with the reference shaders, resets pressure per projection solve, changes the default timestep to `0.125`, and changes mouse drag input to small continuous impulses.
- The GPU correction makes `FluidSettings::useGpu` default to true, wires `Fluid2D::update()` to GPU ping-pong buffers, and keeps CPU as fallback only.

2026-05-10 GPU completion audit:

- `tcxFlowTools_settings`: pass.
- `tcxFlowTools_core_contracts`: pass.
- `example-camera-fluid`: build pass with real `VideoGrabber` texture -> GPU `OpticalFlow` -> GPU `Fluid2D`.
- `example-particles`: build pass with GPU particle state/update/draw default.
- `Fluid2D` obstacle API and generated obstacle passes compile and are covered by tests/contracts.
