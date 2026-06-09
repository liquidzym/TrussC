# tcxFlowTools

`tcxFlowTools` is a TrussC addon for fluid-style creative coding workflows. The fluid solver defaults to a sokol/TrussC GPU ping-pong graph; the CPU path remains only as a robustness fallback when no graphics context or GPU path is available.

The target is a sokol-compatible GPU rewrite of concepts from ofxFlowTools and PixelFlow. Current work is centered on 2D fluid simulation, optical flow, bridges, visualizers, particles, and HD input pipelines; the PixelFlow parity scope also includes Softbody Dynamics, Computational Fluid Dynamics examples, Skylight, post-processing, anti-aliasing, Shadertoy-style shader wrappers, sampling, and geometry/util families.

## Status

Implemented now:

- TrussC addon metadata and CMake integration.
- Public include: `#include <tcxFlowTools.h>`.
- Namespace: `tcx::flow`.
- Settings structs for fluid, optical flow, bridge, and particles.
- `Fluid2D` GPU solver with resize, density/velocity/temperature injection, advection, optional vorticity and buoyancy, divergence, Jacobi pressure solve, projection, and GPU debug visualizers.
- `FluidSettings::resolutionScale` controls simulation resolution, while `outputResolutionScale` can independently size GPU visualization/output FBOs for HD-style low-sim/high-output workflows.
- CPU fallback for `Fluid2D` when `sg_isvalid()` is false, headless mode is active, or `FluidSettings::useGpu` is disabled.
- GPU obstacle mask support through `Fluid2D::addObstacle()` / `clearObstacles()`, with CPU fallback handling for no-GPU contexts.
- `applyVelocityField()` uploads external CPU vector fields as a dynamic RGBA32F texture and blends them into the GPU velocity buffer.
- `OpticalFlow::update(const tc::Texture&, float)` runs a GPU current/previous luminance optical-flow graph for camera/video/texture inputs and exposes `getFlowTexture()`, `getCurrentTexture()`, and `getPreviousTexture()`.
- `example-camera-fluid` connects `tc::VideoGrabber::getTexture()` directly to GPU optical flow, then injects the GPU flow texture into `Fluid2D`.
- Bridge class hierarchy: `BridgeFlow`, `VelocityBridge`, `DensityBridge`, `TemperatureBridge`, `CombinedBridge`, with GPU external texture output for velocity, density, temperature, and combined bridge injection.
- Bridge inputs support ofxFlowTools-style controls: invert, alpha-mask use, mirror-X/Y, mask source selection, soft mask thresholding, and mask gamma.
- Visualization helpers include density/velocity/pressure/temperature views plus styled velocity field arrows, velocity dots, pressure field, and temperature field modes.
- `ParticleFlow` defaults to GPU state textures, GPU update pass, and GPU particle drawing; CPU particles remain only as fallback. First-pass attractor/impulse variants plus per-particle age, lifetime, mass, size spread, and birth-from-velocity controls are available through `ParticleFlowSettings`. GPU spawn initialization now uses shader-side procedural seeds instead of a same-frame CPU seed texture upload.
- `AverageFlow` supports ROI sampling, magnitude normalization, watcher-style magnitude/velocity events, bounded history buffers, and settings serialization for ofxFlowTools average-flow parity.
- `SplitVelocity::updateTexture()` now runs a fuller ofxFlowTools-style GPU graph: raw RGBA positive/negative velocity split, normalized split texture, decayed trail texture, combined/positive/negative/trail visualization output, and split field overlay drawing.
- Extension helper passes include colorize luminance, colorize velocity, colorize gradient, decay, dilate, erode, inverse warp, vector normalization, ease, and first-pass time blur. `FlowHelperPipeline` wraps these fullscreen passes for reusable high-level helper pipelines.
- `SoftBody2D` adds an independent PixelFlow Softbody Dynamics foundation inside tcxFlowTools: Verlet particles, structural/shear/bend constraints, fixed particles, bounds, drag, and constraint cutting.
- `example-simple` with mouse injection and density/velocity/pressure/temperature view switching.
- `example-core-pingpong`, `example-optical-flow`, `example-fluid-bridges`, `example-camera-fluid`, `example-particles`, `example-particle-variants`, `example-average-flow`, `example-lic-streamlines`, `example-split-velocity`, `example-fluid-liquid-text`, `example-fluid-liquid-painting`, `example-fluid-streamlines`, `example-fluid-custom-particles`, `example-fluid-physarum-trails`, `example-fluid-verlet-collision`, `example-softbody2d-cloth`, `example-wind-tunnel`, and `example-hd`.
- Basic tests for settings, resize, density injection, and procedural optical-flow state.

Still limited:

- Exact GPU ping-pong/custom-render streamline parity, richer reference-matched ofx GUI/styling, and richer PixelFlow-style particle variants remain pending.
- PixelFlow Softbody Dynamics has a first independent SoftBody2D cloth implementation; SoftBody2D playground/liquid/collision/differential-growth and SoftBody3D examples remain pending.
- Broader Computational Fluid Dynamics examples beyond liquid text, liquid painting, first-pass streamlines, first-pass custom fluid particles, and first-pass Verlet collision, plus Skylight, post-processing, anti-aliasing, Shadertoy-style wrappers, sampling, and geometry/util families are tracked parity scope but are not yet implemented.
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
- Visualization: `shaders/visualization/visualization.glsl` with scalar, velocity color, pressure, temperature, combined fluid, and first-pass LIC passes.
- Particles: `shaders/particles/particles.glsl` with spawn, birth-from-velocity kick, age/lifespan/mass/size draw layout, first-pass attractor/impulse, and render helper passes.
- Extension APIs: `MouseFlow`, `AverageFlow`, `SplitVelocity`, `ParticleFlow`, and `FlowHelperPipeline`.
- Extension shaders: split velocity raw/visual, normalize vector, decay, colorize luminance/velocity/gradient, dilate, erode, inverse warp, ease, and time blur.

Additional examples:

- `example-optical-flow`: procedural optical flow drives GPU `Fluid2D` through a velocity texture bridge.
- `example-fluid-bridges`: switches velocity, density, temperature, and combined external texture bridges; mode 4 uses a combined visualization of density, velocity, and temperature. Controls include invert, alpha-mask, mask source, soft mask, gamma, and mirror axes.
- `example-wind-tunnel`: GPU fluid obstacle/wind-tunnel example with texture inlet, styled field visualizers, and debug views.
- `example-camera-fluid`: real camera texture drives GPU optical flow and GPU `Fluid2D`; procedural input is fallback only.
- `example-particles`: GPU particle state/update/draw over GPU fluid with age/lifespan/mass/size-spread and birth-from-velocity defaults; CPU particles are fallback only.
- `example-particle-variants`: GPU particle flow, attractor, and impulse modes over GPU fluid with mass-aware movement/draw; keys `1`, `2`, and `3` switch modes. `TCX_PARTICLE_VARIANT=attractor` or `impulse` can start a specific mode for automated visual checks.
- `example-average-flow`: ofxFlowTools `example_extended_average` / `AverageFlowWatcher` first-pass parity example; four ROI regions read back GPU fluid velocity and display average direction, bounded magnitude history, watcher events, and persistent settings.
- `example-lic-streamlines`: GPU LIC texture over `Fluid2D::getVelocityTexture()`; density can be toggled separately.
- `example-split-velocity`: GPU split-velocity helper graph over GPU fluid; keys `1`, `2`, `3`, and `4` switch combined/positive/negative/trail views, with runtime gain, force, decay, and split field overlay controls.
- `example-fluid-liquid-text`: PixelFlow `Fluid_LiquidText` parity example; a generated text FBO is injected into GPU fluid density and temperature, with procedural and mouse velocity disturbance.
- `example-fluid-liquid-painting`: PixelFlow `Fluid_LiquidPainting` parity example; the local PixelFlow Escher image is injected into GPU fluid density while edge flow and mouse drag pull it into liquid-smoke trails.
- `example-fluid-streamlines`: PixelFlow `Fluid_StreamLines` / `FlowField_LIC_StreamLines` first-pass example; GPU fluid velocity is read back to a regular CPU seed grid and drawn as bidirectional streamline segments. Controls cover pause, background mode, velocity vectors, seed particles, seed density, and line length. Exact PixelFlow GPU ping-pong/custom-render parity remains future work.
- `example-fluid-custom-particles`: PixelFlow `Fluid_CustomParticles` first-pass example; `ParticleFlow::spawn()` respawns GPU texture particles into local fluid sources, with left velocity+particles, middle heat+particles, and right particles-only input.
- `example-fluid-physarum-trails`: OpenProcessing `2174194` / GPU-IO Physarum-inspired example; uses tcxFlowTools `Fluid2D` pressure-projected velocity plus GPU particle position/age ping-pong and a GPU trail-deposition pass. It is not a line-by-line GPU-IO layer/program port.
- `example-fluid-verlet-collision`: PixelFlow `Fluid_VerletParticleCollisionSystem` first-pass example; GPU fluid velocity drives Verlet particles with collisions and obstacles. Fluid/particle tuning remains open.
- `example-softbody2d-cloth`: PixelFlow `SoftBody2D_Cloth` parity example; two independent spring cloths hang from fixed top corners with structural/shear/bend constraints, wind, particle dragging, and constraint cutting.
- `example-hd`: GPU fluid with 1x / 0.5x / 0.25x simulation scale and independently toggled output resolution.

2026-06-08 example audit:

- All then-current 18 `examples/example-*` apps rebuilt successfully on macOS/Metal.
- GUI screenshot review covered every example. No black screens, stale texture bindings, or obvious wrong-output defaults were found. `example-simple` is intentionally blank until user injection; `example-fluid-liquid-painting` starts in a readable source-image/liquid-mask state and remains a tuning target rather than a compile/runtime failure.
- The three GPU particle examples were rerun after removing the CPU seed texture upload path; their rerun logs no longer contain the prior `Texture::loadData()` same-frame warning.

2026-06-08 ofxFlowTools deep parity closure:

- Completed the requested deeper ofxFlowTools pass for bridge masks, styled velocity/pressure/temperature field drawing, split-velocity field overlay, AverageFlow history/settings persistence, helper shader pipeline wrappers, and particle birth-from-velocity/layout controls.
- Rebuilt all then-current 18 examples on macOS/Metal after the new source files triggered CMake GLOB refresh.

2026-06-08 OpenProcessing/GPU-IO inspired addition:

- Added `example-fluid-physarum-trails`, based on local source `/Users/mac/Downloads/sketch2174194`. The reference uses GPU-IO velocity/pressure layers, particle aging, RK2 advection, trail fade, and Fluid/Pressure/Velocity render modes. The TrussC example preserves the core effect through existing `Fluid2D` pressure-projected velocity plus GPU particle position/age ping-pong, shader-side velocity sampling, and GPU trail fade/deposition instead of copying the GPU-IO shader graph line by line.
- The Physarum example HUD prints FPS, frame milliseconds, particle count, batch count, current path, and trail vertices. The default path is now `gpu-pingpong + gpu-trail`; the CPU velocity-readback/batched mesh path remains a fallback and comparison path only.
- Final visual alignment notes: the reference is strongly shaped by a low-resolution velocity field, max-clamped additive pointer splats, persistent pressure ping-pong, 1000-frame particle lifetimes, three particle render substeps, fade-only trail accumulation, and dark blue ink on warm paper. The TrussC version reached the accepted visual direction by keeping those invariants while tuning the example-level `flowRangeScale_` and `flowStrengthScale_` so pointer/demo input stays local enough to form readable vortices instead of spreading into a whole-screen smear. `TCX_PHYSARUM_FLOW_RANGE` and `TCX_PHYSARUM_FLOW_STRENGTH` can override the defaults for future project tuning.
- Implementation issues encountered: the first CPU-readback prototype matched the brush/stroke character but was too slow for production particle counts; immediate point drawing could overflow sokol-gl command capacity; a single-channel float age target was not a safe assumption across backends, so the GPU age ping-pong buffer uses `RGBA32F`; and early HUD/path selection used the previous frame's update status, which made trail-length changes appear to fall back to CPU. The final version uses GPU resource readiness for the visible path, keeps `+/-` as a trail-length setting update, and leaves CPU readback as an explicit fallback.
- Remaining follow-up after this addition: PixelFlow Fluid/CFD queue (`VelocityEncoding`, `MultipleFluids`, `TexDataTransfer1/2/3`, custom-render streamlines, exact GPU ping-pong streamlines, `SlowBuoyancy`, Verlet tuning), PixelFlow OpticalFlow capture/movie/PFM work, PixelFlow FlowFieldParticles cohesion/dam-break/sprite-generator work, and exact reference geometry-shader plus byte-for-byte shader-behavior tuning where it is worth the cost.

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

2026-05-12 bridge texture audit:

```bash
cmake --build addons/tcxFlowTools/examples/example-fluid-bridges/build-macos --parallel 2
cmake --build addons/tcxFlowTools/tests/build-macos --parallel 2
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts
```

Result:

- `example-fluid-bridges`: build pass.
- Settings and core-contract tests passed.
- `VelocityBridge`, `DensityBridge`, `TemperatureBridge`, and `CombinedBridge` now accept external GPU texture input and inject the resulting texture output into `Fluid2D`.
- Visual check: modes 1, 2, 3, and 4 switch between velocity, density, temperature, and combined views; mode 2 is no longer black and mode 4 uses a live combined visualization rather than a stale previous-frame FBO.
- Known linker warning remains: duplicate `libTrussC.a` in the example link line.

2026-05-13 LIC visualization audit:

```bash
cmake -S addons/tcxFlowTools/examples/example-lic-streamlines -B addons/tcxFlowTools/examples/example-lic-streamlines/build-macos
cmake --build addons/tcxFlowTools/examples/example-lic-streamlines/build-macos --parallel 2
cmake --build addons/tcxFlowTools/tests/build-macos --parallel 2
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts
```

Result:

- `example-lic-streamlines`: configure/build pass.
- Settings and core-contract tests passed.
- Visual check: `Fluid2D::drawLic()` renders a nonblank LIC-style flow texture over GPU velocity; `l` toggles LIC and `d` toggles density.
- Known linker warning remains: duplicate `libTrussC.a` in the example link line.

2026-05-13 particle variant audit:

```bash
cmake -S addons/tcxFlowTools/examples/example-particle-variants -B addons/tcxFlowTools/examples/example-particle-variants/build-macos
cmake --build addons/tcxFlowTools/examples/example-particle-variants/build-macos --parallel 2
cmake --build addons/tcxFlowTools/examples/example-particles/build-macos --parallel 2
cmake --build addons/tcxFlowTools/tests/build-macos --parallel 2
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts
```

Result:

- `example-particle-variants`: configure/build pass.
- `example-particles`: regression build pass.
- Settings and core-contract tests passed.
- Visual check: `TCX_PARTICLE_VARIANT=flow`, `attractor`, and `impulse` each launched the GPU path and rendered mode-specific particle behavior.
- Known linker warning remains: duplicate `libTrussC.a` in the example link line.

2026-05-13 HD output-resolution audit:

```bash
cmake --build addons/tcxFlowTools/examples/example-hd/build-macos --parallel 2
cmake --build addons/tcxFlowTools/tests/build-macos --parallel 2
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts
```

Result:

- `example-hd`: build pass.
- Settings and core-contract tests passed.
- `Fluid2D` now reports separate simulation and output dimensions.
- Visual check: `TCX_HD_SCALE=0.25 TCX_HD_OUTPUT_SCALE=1.0` rendered a 320x180 simulation through a 1280x720 output FBO; `TCX_HD_OUTPUT_SCALE=0.5` rendered a 640x360 output FBO.
- Known linker warning remains: duplicate `libTrussC.a` in the example link line.

2026-05-13 split-velocity helper audit:

```bash
cmake -S addons/tcxFlowTools/examples/example-split-velocity -B addons/tcxFlowTools/examples/example-split-velocity/build-macos
cmake --build addons/tcxFlowTools/examples/example-split-velocity/build-macos --parallel 2
cmake --build addons/tcxFlowTools/tests/build-macos --parallel 2
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts
```

Result:

- `example-split-velocity`: configure/build pass.
- Settings and core-contract tests passed.
- Visual check: `TCX_SPLIT_MODE=0`, `1`, and `2` each launched the GPU path and rendered combined, positive, and negative split-velocity views.
- Known linker warning remains: duplicate `libTrussC.a` in the example link line.

2026-05-13 PixelFlow liquid-text CFD audit:

```bash
cmake -S addons/tcxFlowTools/examples/example-fluid-liquid-text -B addons/tcxFlowTools/examples/example-fluid-liquid-text/build-macos
cmake --build addons/tcxFlowTools/examples/example-fluid-liquid-text/build-macos --parallel 2
cmake --build addons/tcxFlowTools/tests/build-macos --parallel 2
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts
```

Result:

- `example-fluid-liquid-text`: configure/build pass.
- Settings and core-contract tests passed.
- Visual check: `TCX_LIQUID_TEXT_SOURCE=1` launched the GPU path, rendered the generated text source preview, and showed the same text being injected into the live fluid density/temperature field.
- macOS keyboard event injection was blocked by system permissions during automation, so the source/combined toggles were verified through startup state plus visual inspection rather than synthetic keypresses.
- Known linker warning remains: duplicate `libTrussC.a` in the example link line.

2026-05-13 PixelFlow SoftBody2D cloth audit:

```bash
cmake -S addons/tcxFlowTools/examples/example-softbody2d-cloth -B addons/tcxFlowTools/examples/example-softbody2d-cloth/build-macos
cmake --build addons/tcxFlowTools/examples/example-softbody2d-cloth/build-macos --parallel 2
cmake --build addons/tcxFlowTools/tests/build-macos --parallel 2
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_softbody2d
```

Result:

- `example-softbody2d-cloth`: configure/build pass.
- Settings, core-contract, and SoftBody2D tests passed.
- Visual check: two cloths render with PixelFlow `SoftBody2D_Cloth` core behavior: fixed top corners, visible particle/spring grid, shear/bend support, gravity sag, and wind deformation. User confirmed the visual direction as correct.
- Known linker warning remains: duplicate `libTrussC.a` in the example link line.

2026-05-13 PixelFlow liquid-painting CFD audit:

```bash
cmake -S addons/tcxFlowTools/examples/example-fluid-liquid-painting -B addons/tcxFlowTools/examples/example-fluid-liquid-painting/build-macos
cmake --build addons/tcxFlowTools/examples/example-fluid-liquid-painting/build-macos --parallel 2
cmake --build addons/tcxFlowTools/tests/build-macos --parallel 2
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_softbody2d
```

Result:

- `example-fluid-liquid-painting`: configure/build pass.
- Visual reference capture used PixelFlow Vimeo `184849892` with `yt-dlp`/`ffmpeg` into `/tmp/tcxFlowTools-reference-captures`; captured assets are not committed.
- Visual check: `TCX_LIQUID_PAINTING_SOURCE=1` keeps the Escher source readable while procedural edge flow and mouse drag pull it into PixelFlow-style liquid-smoke trails. User confirmed the effect is correct.
- Known linker warning remains: duplicate `libTrussC.a` in the example link line.
