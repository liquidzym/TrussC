# Phase Gates

Generated: 2026-05-10

This file is the strict review ledger for `tcxFlowTools`. A phase is only treated as complete when it has:

- A clear implementation summary.
- A review checklist with pass/fail status.
- At least one test or example command.
- Known limitations moved into `KNOWN_LIMITATIONS.md`.
- A note on whether TrussC core API changed.

## Phase Status

| Phase | Scope | Required proof | Status |
|---|---|---|---|
| Phase 1 | Addon scaffolding, CMake, public include, docs | `tests`, `example-simple` configure/build | Complete with limitations |
| Phase 2 | GPU/resource infrastructure: ping-pong buffers, common pass wrappers, texture format helpers, shader directory | `test_core_contracts`, `example-simple`, `example-core-pingpong` configure/build | Complete for common passes |
| Phase 3 | `Fluid2D` solver | `example-simple` with visible fluid; solver tests | Complete with GPU default and CPU fallback |
| Phase 4 | Optical flow | `example-optical-flow` and nonzero motion field | Complete with GPU texture input and GPU fluid bridge |
| Phase 5 | Bridge modules | `example-fluid-bridges`, camera/video texture input | Complete for GPU external texture bridge injection |
| Phase 6 | Visualization/debug | debug visualizer example toggles | Complete with GPU fluid, combined, and first-pass LIC visualizers |
| Phase 7 | Extensions/particles | `example-particles`, `example-particle-variants` | Complete with GPU particles default and first-pass variants |
| Phase 8 | HD pipeline | `example-hd` with independent sim/output scale | Complete with GPU fluid and separate output scale |
| Phase 9 | Final tests/docs cleanup | all selected examples build; docs updated | Complete for GPU fluid milestone |
| Phase 10 | ofx helper shader gaps | split velocity / average watcher / helper filters | Complete for first-pass ofx deep parity |
| Phase 11 | PixelFlow CFD + flow-field examples | liquid text/painting, streamlines, collision, velocity encoding | In progress with liquid text and liquid painting |
| Phase 12 | PixelFlow Softbody Dynamics | SoftBody2D/3D cloth, chains, collision, playground | In progress with first SoftBody2D cloth example |
| Phase 13 | Broader PixelFlow modules | Skylight, post-processing, anti-aliasing, Shadertoy, sampling/geometry | Planned |

## Phase 1 Review

Implementation summary:

- Added `addon.json`, `CMakeLists.txt`, `src/tcxFlowTools.h`.
- Added initial module layout under `src/tcxFlow`.
- Added migration, HD, portability, license, and limitations docs.
- Added `example-simple`.

Review checklist:

- Public include works: pass.
- Addon can be loaded by `addons.make`: pass.
- Minimal example builds: pass on macOS.
- TrussC core API changed: no.
- Limitations documented: pass.

Review commands:

```bash
cmake -S addons/tcxFlowTools/tests -B addons/tcxFlowTools/tests/build-macos
cmake --build addons/tcxFlowTools/tests/build-macos --parallel
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings

cmake -S addons/tcxFlowTools/examples/example-simple -B addons/tcxFlowTools/examples/example-simple/build-macos
cmake --build addons/tcxFlowTools/examples/example-simple/build-macos --parallel
```

## Phase 2 Review

Implementation summary:

- `PingPongBuffer` manages two TrussC `Fbo` resources, supports allocate, resize, clear, swap, release, move-only semantics.
- `TextureUtils` provides conservative renderable format selection.
- `FlowPass` now maps built-in common passes to generated sokol-shdc descriptors, binds up to two input textures by name, and lazy-loads the shader on first render.
- `shaders/common/common.glsl` implements copy, clear, multiply, threshold, luminance, difference, blur horizontal, and blur vertical passes with a shared fullscreen quad layout.
- Addon CMake compiles `shaders/**/*.glsl` with the same sokol-shdc flow used by TrussC examples and existing addons.
- `example-core-pingpong` visually exercises allocation, clear, resize, swap, plus generated `FlowPass` copy/clear previews.

Review checklist:

- Resource wrapper is move-only: pass by compile-time test.
- Texture format helper returns a valid TrussC format: pass by test.
- Visual ping-pong/common-pass example builds: pass on macOS.
- Common shader source compiles for `metal_macos:hlsl5:glsl300es:wgsl`: pass after aligning with TrussC shader declarations and ensuring WGSL-visible uniform usage.
- FlowPass pass-kind mapping, generated shader header presence, and common parameter defaults: pass by `test_core_contracts`.
- `example-simple` and `example-core-pingpong` build with the addon shader target: pass on macOS.
- TrussC core API changed: no.

Review commands:

```bash
cmake -S addons/tcxFlowTools/tests -B addons/tcxFlowTools/tests/build-macos
cmake --build addons/tcxFlowTools/tests/build-macos --parallel
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts

cmake -S addons/tcxFlowTools/examples/example-simple -B addons/tcxFlowTools/examples/example-simple/build-macos
cmake --build addons/tcxFlowTools/examples/example-simple/build-macos --parallel

cmake -S addons/tcxFlowTools/examples/example-core-pingpong -B addons/tcxFlowTools/examples/example-core-pingpong/build-macos
cmake --build addons/tcxFlowTools/examples/example-core-pingpong/build-macos --parallel
```

Audit note, 2026-05-10:

- Reference checked: TrussC `fullscreenShaderExample`, `tcxHap`, and `tcxLut` shader usage.
- The common pass shader intentionally uses a quad vertex buffer through `tc::FullscreenShader`; it does not assume `gl_VertexID`.
- WGSL backend rejects a fragment uniform block when none of its values are used in that fragment stage, so texture passes that otherwise ignore color now multiply by the default white color. This keeps copy/luminance behavior unchanged at default settings while preserving cross-backend generation.

Review result on 2026-05-10:

- `tcxFlowTools_settings`: pass.
- `tcxFlowTools_core_contracts`: pass.
- `example-core-pingpong`: configure/build pass.
- `example-simple` regression build: pass.
- Linker warning observed: duplicate `libTrussC.a`, consistent with current addon/app linkage and not specific to this phase.

## Phase 3 Review

Implementation summary:

- `Fluid2D::update()` now runs a GPU-first fluid path:
  - semi-Lagrangian velocity advection,
  - density advection,
  - temperature advection,
  - optional buoyancy,
  - velocity texture injection,
  - optional vorticity confinement,
  - reference-aligned divergence calculation,
  - pressure reset and Jacobi pressure solve,
  - gradient subtraction / projection,
  - CPU fallback when no graphics context is available.
- `Fluid2D::drawDensity()` draws the GPU density FBO when the GPU solver is active; CPU fallback still uploads a dynamic linear-filtered `tc::Image`.
- `FluidBuffers` now groups Phase 3 GPU resources: velocity, density, temperature, pressure ping-pong buffers plus divergence and curl targets.
- `shaders/fluid/fluid.glsl` now provides generated sokol-shdc passes for advect, splat, add velocity texture, divergence, Jacobi pressure, gradient subtract, vorticity curl, vorticity force, and buoyancy.
- `FlowPassKind` maps the generated fluid pass descriptors so Phase 3 GPU passes can be loaded through the same fullscreen pass wrapper as common passes.
- `example-simple` remains the visual Phase 3 smoke test.
- Numeric tests assert density, velocity, temperature, pressure, and finite energy after update; core-contract tests verify FluidBuffers ownership and fluid pass readiness.

Review checklist:

- Mouse density/velocity injection API exists: pass.
- Solver iterations affect pressure solve loop: pass by code path.
- Resize preserves valid simulation dimensions: pass by test.
- Output energy remains finite after update: pass by test.
- Density view no longer exposes enlarged simulation-cell rectangles: pass by code review and regression build.
- `FluidBuffers.h` deliverable exists and is move-only: pass by compile-time test.
- Fluid shader deliverable compiles for `metal_macos:hlsl5:glsl300es:wgsl`: pass by build.
- Fluid generated pass mapping is reachable through `FlowPassKind`: pass by `test_core_contracts`.
- `example-simple` builds after solver change: pass.
- Full GPU implementation: pass for the fluid solver milestone. `Fluid2D::update()` uses GPU buffers by default and falls back to CPU only when GPU use is unavailable or disabled.
- TrussC core API changed: no.

Review commands:

```bash
cmake --build addons/tcxFlowTools/tests/build-macos --parallel
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts
cmake --build addons/tcxFlowTools/examples/example-simple/build-macos --parallel
cmake --build addons/tcxFlowTools/examples/example-core-pingpong/build-macos --parallel
```

Review result on 2026-05-10:

- `tcxFlowTools_settings`: pass.
- `tcxFlowTools_core_contracts`: pass.
- `example-simple`: build pass.
- `example-core-pingpong`: regression build pass.
- `example-optical-flow`, `example-fluid-bridges`, `example-camera-fluid`, `example-particles`, `example-hd`: regression build pass.

Audit note, 2026-05-10:

- Reference checked: PixelFlow `advect.frag`, `divergence.frag`, `jacobi.frag`, `gradient.frag`, `vorticityCurl.frag`, `vorticityForce.frag`, `buoyancy.frag`, plus ofxFlowTools `ftAdvectShader.h` and `ftVorticityForceShader.h`.
- The shader graph keeps the reference sign convention: positive divergence, Jacobi `(neighbors - divergence) * 0.25`, and gradient subtraction.
- TrussC/sokol generated headers expose file-local static shader sources, so `fluid.glsl` uses unique vertex-stage and texture names to avoid collisions when included beside `common.glsl.h`.

Visual correction audit on 2026-05-10:

- Issue observed: `example-simple` appeared as large square blocks in the provided screenshot.
- Root cause: `drawDensity()` drew each simulation cell as a scaled rectangle, and `example-simple` used 0.25 simulation scale.
- Correction: density display now renders through a linear-filtered `tc::Image`; `example-simple` now uses 0.5 simulation scale, higher solver iterations, softer density injection, diffusion, and vorticity.
- TrussC core API changed: no.

Fluid-character correction audit on 2026-05-10:

- Issue observed: after density display smoothing, `example-simple` still behaved like dragged color blocks rather than fluid.
- Reference comparison: ofxFlowTools and PixelFlow both use positive divergence followed by Jacobi pressure solve with negative divergence contribution, then subtract pressure gradient.
- Root cause: the earlier solver path had divergence sign inverted while keeping the reference Jacobi form, used a much smaller advection timestep than PixelFlow/ofxFlowTools, injected oversized mouse density spots, and used a non-reference vorticity force direction.
- Correction: divergence/projection signs were aligned with reference shaders, pressure is reset before each projection solve, default timestep is `0.125`, vorticity curl/force direction follows PixelFlow/ofxFlowTools, `MouseFlow` emits small continuous drag impulses along the mouse segment, and the default app path now runs the GPU graph.
- TrussC core API changed: no.

## Phase 4 Review

Implementation summary:

- `OpticalFlow` keeps previous/current CPU luminance frames and estimates flow from temporal difference plus spatial gradients.
- `updateProcedural()` now generates a moving procedural luminance texture, applies optional blur, computes thresholded optical flow, and applies temporal smoothing/decay.
- `shaders/opticalflow/opticalflow.glsl` provides generated sokol-shdc passes for luminance, difference, gradient, optical flow, temporal smoothing, and visualization.
- `FlowPassKind` maps the generated optical-flow pass descriptors through the shared fullscreen pass wrapper.
- `Fluid2D::applyVelocityField()` uploads CPU optical-flow vectors to a dynamic RGBA32F texture and blends that texture into the GPU fluid velocity buffer when graphics is available.
- `example-optical-flow` demonstrates flow visualization and flow-driven fluid.

Review checklist:

- Procedural moving luminance texture exists: pass by `currentFrameEnergy()` test.
- Nonzero optical-flow field exists: pass by `flowEnergy()` test.
- Optical-flow shader deliverable compiles for `metal_macos:hlsl5:glsl300es:wgsl`: pass by build.
- Optical-flow generated pass mapping is reachable through `FlowPassKind`: pass by `test_core_contracts`.
- Optical flow drives fluid velocity: pass by `Fluid2D::applyVelocityField()` test and `example-optical-flow`.
- Visual example builds: pass on macOS.
- Real camera/video texture sampling through the optical-flow graph: pass by `OpticalFlow::update(const tc::Texture&, float)` and `example-camera-fluid`.
- TrussC core API changed: no.

Review commands:

```bash
cmake --build addons/tcxFlowTools/tests/build-macos --parallel
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts
cmake --build addons/tcxFlowTools/examples/example-optical-flow/build-macos --parallel 2
```

Review result on 2026-05-10:

- `tcxFlowTools_settings`: pass.
- `tcxFlowTools_core_contracts`: pass.
- `example-optical-flow`: build pass.

Audit note, 2026-05-10:

- Reference checked: PixelFlow `OpticalFlow.frag`, `OpticalFlowGray.frag`, `Filter/sobel.frag`, and ofxFlowTools `ftOpticalFlowShader.h`.
- The procedural optical-flow estimator follows the same structure as the references: current/previous luminance difference, spatial gradient, threshold, scale, then temporal smoothing.
- The optical shader uses a unique vertex-stage name and `color/resolution/texel/options` uniform layout to stay compatible with the shared `FlowPassParams` block and avoid generated-header symbol collisions.

GPU texture-input audit on 2026-05-10:

- `OpticalFlow` now allocates current, previous, raw-flow, and smoothed-flow GPU buffers.
- `update(const tc::Texture&, float)` runs luminance, optical-flow, and temporal-smooth passes fully on GPU when a graphics context is valid.
- `getFlowTexture()`, `getCurrentTexture()`, and `getPreviousTexture()` expose the GPU outputs for direct fluid/visualizer wiring.
- CPU procedural flow remains for tests and no-GPU fallback only.

## Phase 5 Review

Implementation summary:

- Bridge hierarchy has `VelocityBridge`, `DensityBridge`, `TemperatureBridge`, and `CombinedBridge`.
- Bridges support procedural `update(dt)` fallback when no texture/camera is available.
- Bridges support GPU external texture input through `update(const tc::Texture&, float)` and render bridge outputs into RGBA32F FBOs before applying them to `Fluid2D`.
- `shaders/bridge/bridge.glsl` provides generated luminance mask, velocity, density, and temperature bridge passes.
- `FlowPassKind` maps bridge pass descriptors through the shared fullscreen pass wrapper.
- `example-fluid-bridges` switches modes 1-4 across individual and combined bridges with a dynamic texture source.
- `example-camera-fluid` connects real `tc::VideoGrabber` texture input to `OpticalFlow::update(texture)`, then applies `getFlowTexture()` to GPU `Fluid2D`.

Review checklist:

- Bridge modules compile and apply to `Fluid2D`: pass.
- Combined bridge can apply velocity/density/temperature together: pass.
- Bridge shader deliverable compiles for `metal_macos:hlsl5:glsl300es:wgsl`: pass by build.
- Bridge generated pass mapping is reachable through `FlowPassKind`: pass by `test_core_contracts`.
- Bridge example builds: pass on macOS.
- Camera-fluid GPU-fluid example builds: pass on macOS.
- External texture bridge processing: pass for velocity, density, temperature, and combined bridge injection.
- Later closure note: advanced bridge settings such as invert, alpha-mask, mirror axes, mask source, soft threshold, and gamma were implemented on 2026-06-08.
- TrussC core API changed: no.

Review commands:

```bash
cmake --build addons/tcxFlowTools/examples/example-fluid-bridges/build-macos --parallel 2
cmake --build addons/tcxFlowTools/examples/example-camera-fluid/build-macos --parallel 2
```

Review result on 2026-05-10:

- `example-fluid-bridges`: build pass.
- `example-camera-fluid`: build pass.

Bridge texture audit on 2026-05-12:

- `cmake --build addons/tcxFlowTools/examples/example-fluid-bridges/build-macos --parallel 2`: pass, with the known duplicate `libTrussC.a` linker warning.
- `cmake --build addons/tcxFlowTools/tests/build-macos --parallel 2`: pass.
- `addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings`: pass.
- `addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts`: pass.
- Visual check: mode 1 shows velocity, mode 2 shows density and is no longer black, mode 3 shows temperature, and mode 4 shows live combined density/velocity/temperature output.
- Reference checked: ofxFlowTools bridge class split and PixelFlow filter primitives.
- Later closure note: advanced bridge controls such as invert, alpha-mask, mirror axes, mask source, soft threshold, and gamma were implemented on 2026-06-08. Exact ofx GUI parameter binding remains future tuning.

## Phase 6 Review

Implementation summary:

- `Fluid2D` exposes debug draw methods for density, velocity, pressure, temperature, combined density/velocity/temperature, and LIC over velocity; GPU fluid uses generated visualization passes, CPU fallback uses immediate/debug drawing.
- `OpticalFlow` exposes `drawFlow()` and `drawDebug()`.
- `FlowVisualizer` routes fluid and optical-flow debug drawing.
- `shaders/visualization/visualization.glsl` provides generated scalar, velocity color, pressure, temperature, combined, and LIC visualization passes.
- `FlowPassKind` maps visualization pass descriptors through the shared fullscreen pass wrapper.
- `example-simple`, `example-optical-flow`, `example-fluid-bridges`, `example-lic-streamlines`, and `example-hd` exercise visualization paths.

Review checklist:

- Density visualization: pass.
- Velocity visualization: pass.
- Pressure visualization: pass in `example-simple` mode `p`.
- Temperature visualization: pass in `example-simple` mode `t`.
- Combined visualization: pass in `example-fluid-bridges` mode `4`.
- LIC visualization: pass in `example-lic-streamlines`.
- Visualization shader deliverable compiles for `metal_macos:hlsl5:glsl300es:wgsl`: pass by build.
- Visualization generated pass mapping is reachable through `FlowPassKind`: pass by `test_core_contracts`.
- Shader visualizers are used for GPU fluid debug views: pass.
- TrussC core API changed: no.

Review result on 2026-05-10:

- Visualization paths compile through all current examples.

LIC visualization audit on 2026-05-13:

- `cmake -S addons/tcxFlowTools/examples/example-lic-streamlines -B addons/tcxFlowTools/examples/example-lic-streamlines/build-macos`: pass.
- `cmake --build addons/tcxFlowTools/examples/example-lic-streamlines/build-macos --parallel 2`: pass, with the known duplicate `libTrussC.a` linker warning.
- `cmake --build addons/tcxFlowTools/tests/build-macos --parallel 2`: pass.
- `addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings`: pass.
- `addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts`: pass.
- Visual check: `example-lic-streamlines` renders a nonblank LIC-style texture aligned to the GPU velocity field.
- Remaining gap: full PixelFlow-style streamline particle rendering is not yet implemented.

## Phase 7 Review

Implementation summary:

- `ParticleFlow` defaults to GPU particle state textures over GPU fluid.
- GPU particles update against `Fluid2D::getVelocityTexture()` and draw through a generated vertex texture sampling shader.
- `ParticleFlowSettings::variant` supports first-pass flow, attractor, and impulse modes. The GPU update shader uses the configured normalized variant center and strength; CPU fallback mirrors the same force direction.
- CPU particles still sample `Fluid2D::sampleVelocityAtPosition()` as no-GPU fallback.
- `AverageFlow` computes sampled average velocity and speed from a `Fluid2D` field.
- `SplitVelocity` computes sampled positive/negative velocity channels and horizontal/vertical energy.
- `shaders/particles/particles.glsl` provides generated spawn, update, render helper, and point-draw passes.
- `FlowPassKind` maps particle pass descriptors through the shared fullscreen pass wrapper.
- `example-particles` demonstrates particle motion with optional fluid overlay.
- `example-particle-variants` demonstrates flow, attractor, and impulse modes over GPU fluid.

Review checklist:

- Particle settings exist: pass.
- Particle reset exists: pass.
- Particles sample velocity field: pass by GPU velocity texture path and CPU fallback implementation.
- AverageFlow and SplitVelocity APIs compile and produce finite sampled values: pass by `tcxFlowTools_settings`.
- Particle shader deliverable compiles for `metal_macos:hlsl5:glsl300es:wgsl`: pass by build.
- Particle generated pass mapping is reachable through `FlowPassKind`: pass by `test_core_contracts`.
- Particle example builds: pass on macOS.
- Full GPU particle simulation/rendering: pass for the default particle path; advanced reference variants remain tracked in `REFERENCE_GAPS.md`.
- First-pass attractor and impulse variants: pass in `example-particle-variants`.
- TrussC core API changed: no.

Review command:

```bash
cmake --build addons/tcxFlowTools/examples/example-particles/build-macos --parallel 2
```

Review result on 2026-05-10:

- `example-particles`: build pass.

Audit note, 2026-05-10:

- Reference checked: ofxFlowTools `ftParticleFlow`, particle init/move/draw shaders, and PixelFlow `FlowFieldParticles` examples.
- Current implementation intentionally ports the default GPU state/update/draw shape first. It does not yet claim parity with every PixelFlow particle variant such as attractors, cohesion, sprite trails, or dam break.

Particle variant audit on 2026-05-13:

- `cmake -S addons/tcxFlowTools/examples/example-particle-variants -B addons/tcxFlowTools/examples/example-particle-variants/build-macos`: pass.
- `cmake --build addons/tcxFlowTools/examples/example-particle-variants/build-macos --parallel 2`: pass, with the known duplicate `libTrussC.a` linker warning.
- `cmake --build addons/tcxFlowTools/examples/example-particles/build-macos --parallel 2`: pass, with the known duplicate `libTrussC.a` linker warning.
- `cmake --build addons/tcxFlowTools/tests/build-macos --parallel 2`: pass.
- `addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings`: pass.
- `addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts`: pass.
- Visual check: `TCX_PARTICLE_VARIANT=flow`, `attractor`, and `impulse` each launched on the GPU path and rendered a nonblank mode-specific particle field.
- Remaining gap: richer PixelFlow particle families such as cohesion, dam-break, sprite trails, and optical-flow capture particles are still not implemented.

## Phase 8 Review

Implementation summary:

- `FluidSettings::resolutionScale` controls input/display-to-simulation downscaling.
- `FluidSettings::outputResolutionScale` controls GPU visualization/output FBO scale independently from simulation scale. A value <= 0 follows `resolutionScale` for backward-compatible behavior.
- `Fluid2D` exposes `outputWidth()` and `outputHeight()` alongside simulation dimensions.
- `example-hd` supports 1x, 0.5x, and 0.25x simulation scale with independently toggled 1x/0.5x output scale.

Review checklist:

- 1x / 0.5x / 0.25x switching exists: pass.
- Separate output-resolution scale exists: pass by `FluidSettings::outputResolutionScale`, `Fluid2D::outputWidth()`, and `Fluid2D::outputHeight()`.
- Resize rebuilds simulation dimensions: pass by implementation and build.
- HD example builds: pass on macOS.
- GPU fluid display at multiple simulation scales: pass.
- GPU visualization output at independent resolution: pass.
- TrussC core API changed: no.

Review command:

```bash
cmake --build addons/tcxFlowTools/examples/example-hd/build-macos --parallel 2
```

Review result on 2026-05-10:

- `example-hd`: build pass.

HD output-resolution audit on 2026-05-13:

- `cmake --build addons/tcxFlowTools/examples/example-hd/build-macos --parallel 2`: pass, with the known duplicate `libTrussC.a` linker warning.
- `cmake --build addons/tcxFlowTools/tests/build-macos --parallel 2`: pass.
- `addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings`: pass.
- `addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts`: pass.
- Visual check: `TCX_HD_SCALE=1.0 TCX_HD_OUTPUT_SCALE=1.0`, `TCX_HD_SCALE=0.25 TCX_HD_OUTPUT_SCALE=1.0`, and `TCX_HD_SCALE=0.25 TCX_HD_OUTPUT_SCALE=0.5` each rendered nonblank GPU fluid output with HUD dimensions matching the requested simulation/output split.

## Phase 9 Review

Current review result on 2026-05-10:

- `tcxFlowTools_settings`: pass.
- `tcxFlowTools_core_contracts`: pass.
- Examples centralized under `addons/tcxFlowTools/examples/`: pass.
- `example-simple`: build pass.
- `example-core-pingpong`: build pass.
- `example-optical-flow`: build pass.
- `example-fluid-bridges`: build pass.
- `example-camera-fluid`: build pass.
- `example-particles`: build pass.
- `example-hd`: build pass.

Phase 9 is complete for the current GPU-first milestone. CPU remains a no-GPU fallback. Remaining reference gaps now include exact GPU ping-pong/custom-render streamlines, exact HD shader-family parity beyond output resolution, richer particle variants, broader PixelFlow CFD examples, PixelFlow Softbody Dynamics, Skylight/PostProcessing/AntiAliasing/Shadertoy/sampling/geometry modules, and cross-platform builds; see `REFERENCE_GAPS.md`.

## Phase 10 Review

Implementation summary:

- `SplitVelocity` still computes CPU sampled positive/negative velocity metrics.
- `SplitVelocity::updateTexture()` now runs a fuller GPU graph: raw RGBA positive/negative velocity split, normalized split texture, decayed trail texture, visual output, and split field overlay drawing.
- `shaders/extensions/extensions.glsl` adds generated split-velocity raw/visual, normalize vector, decay, colorize luminance/velocity/gradient, dilate, erode, inverse warp, ease, and time blur helper passes.
- `FlowPassKind` maps the generated extension helper passes through the shared fullscreen pass wrapper.
- `FlowHelperPipeline` wraps the generated helper passes into reusable high-level fullscreen pipelines.
- `example-split-velocity` demonstrates combined/positive/negative/trail split views over GPU fluid, with runtime gain, force, decay, and field overlay controls.
- `AverageFlow` now supports ROI sampling, magnitude normalization, watcher-style magnitude/velocity events, bounded history, and settings serialization.
- `example-average-flow` demonstrates first-pass ofxFlowTools `example_extended_average` / `AverageFlowWatcher` parity over a GPU fluid field with four ROI overlays and persistent settings.
- Bridge and particle deep ofx gaps are included in this phase: bridge mask source/softness/gamma controls and particle birth-from-velocity/layout controls are implemented.

Review checklist:

- Split-velocity and helper shaders compile for `metal_macos:hlsl5:glsl300es:wgsl`: pass by build.
- Generated extension shader header exists and pass mapping is reachable through `FlowPassKind`: pass by `test_core_contracts`.
- Existing CPU split metrics still run: pass by `tcxFlowTools_settings`.
- Visual example builds: pass on macOS.
- Visual example renders nonblank combined/positive/negative/trail GPU views: pass by 2026-06-08 screenshot audit.
- First-pass `AverageFlowWatcher`, split field overlay, helper-pipeline wrappers, deep bridge masks, and particle birth-from-velocity parity exist. Full ofx helper parity remains partial for exact GUI panels, XML format, and reference geometry styling.
- TrussC core API changed: no.

Review commands:

```bash
cmake -S addons/tcxFlowTools/examples/example-split-velocity -B addons/tcxFlowTools/examples/example-split-velocity/build-macos
cmake --build addons/tcxFlowTools/examples/example-split-velocity/build-macos --parallel 2
cmake -S addons/tcxFlowTools/examples/example-average-flow -B addons/tcxFlowTools/examples/example-average-flow/build-macos
cmake --build addons/tcxFlowTools/examples/example-average-flow/build-macos --parallel 2
cmake --build addons/tcxFlowTools/tests/build-macos --parallel 2
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts
```

## Phase 11+ Scope Note

2026-05-13 clarification:

- The visual references are the ofxFlowTools Vimeo example and PixelFlow's Vimeo/example index. The goal is matching the core visual effect family, not only compiling similarly named APIs.
- PixelFlow Softbody Dynamics and Computational Fluid Dynamics examples are explicitly in scope.
- Broader PixelFlow modules such as Skylight, PostProcessing Filters, AntiAliasing, Shadertoy wrappers, sampling, and geometry/util examples are also in parity scope unless a later task splits them into companion addons.

2026-06-08 execution queue:

- Start with ofxFlowTools parity: bridge invert/alpha-mask/mirror controls, velocity dots/field and pressure/temperature styling, full split-velocity graph, helper shaders, and particle age/lifespan/mass/draw/move details.
- Continue with PixelFlow Fluid/CFD: velocity encoding, multiple fluids, texture data transfer examples, custom-render streamlines, exact GPU ping-pong streamlines, slow buoyancy, and Verlet collision tuning.
- Then continue with PixelFlow OpticalFlow: capture/movie fluid, capture-driven Verlet particles, and PFM export.
- Then continue with PixelFlow FlowFieldParticles: cohesion, dam break, optical-flow capture particles, and sprite generator.
- After the missing PixelFlow queue is covered, revisit exact ofx geometry-shader/mesh details and pixel-level shader behavior where the extra fidelity matters.

2026-06-08 progress:

- First-pass ofxFlowTools bridge controls implemented: invert, alpha-mask use, mirror-X, and mirror-Y are packed into the bridge shader option flags and exposed by `example-fluid-bridges`.
- First-pass ofxFlowTools field visualizers implemented: `FlowVisualizer` supports velocity field arrows, velocity dots, pressure field, and temperature field modes, with `example-wind-tunnel` controls.
- Fuller ofxFlowTools split-velocity graph implemented: raw split, normalized split, decayed trail, and visual output are wired through `SplitVelocity`.
- First-pass ofxFlowTools helper shader suite implemented: colorize luminance/velocity/gradient, decay, dilate, erode, inverse warp, normalization, ease, and time blur are available as generated `FlowPassKind` entries.
- First-pass ofxFlowTools particle details implemented: `ParticleFlowSettings` exposes lifespan spread, mass, mass spread, and size spread; GPU and CPU movement/draw paths use per-particle mass and age/lifespan fade.
- Deep ofxFlowTools closure added later on 2026-06-08: bridge mask source/softness/gamma, styled pressure/temperature/velocity field drawing, split-velocity field overlay, AverageFlow history/settings serialization, `FlowHelperPipeline`, and particle birth-from-velocity/layout controls.
- Particle GPU spawn initialization now uses procedural shader seeds instead of a CPU seed texture upload, fixing the same-frame `Texture::loadData()` warning seen in the three particle examples.
- User-reported builds for `example-fluid-liquid-painting` and `example-softbody2d-cloth` were reconfigured and built successfully; no source failure was reproduced.
- All then-current 18 `examples/example-*` apps were reconfigured and built successfully on macOS/Metal.
- Example review finding fixed: `example-simple` now explicitly handles the expanded `FlowVisualizer::Mode` enum in its draw switch.
- GUI screenshot review covered all then-current 18 examples. No black screens, stale texture bindings, or obvious wrong-output defaults were found in the sampled startup states. `example-simple` remains intentionally empty until drag injection.

## Phase 11 Review

Implementation summary:

- Added `example-fluid-liquid-text`, based on PixelFlow `Fluid_LiquidText`.
- The example renders a Processing/Fluid/Simulation-style text source into a TrussC FBO, injects that texture into GPU fluid density and temperature, and adds procedural plus mouse velocity disturbance.
- `TCX_LIQUID_TEXT_SOURCE=1` shows a source-preview panel for visual parity checks. `TCX_LIQUID_TEXT_DENSITY` and `TCX_LIQUID_TEXT_TEMPERATURE` expose startup tuning for automated visual runs.
- Added `example-fluid-liquid-painting`, based on PixelFlow `Fluid_LiquidPainting`. It loads the local PixelFlow Escher image, injects it into GPU density, and uses procedural edge flow plus mouse drag to create liquid-smoke image smearing.
- `TCX_LIQUID_PAINTING_SOURCE=1` shows the injected source preview. `TCX_LIQUID_PAINTING_MIX=<float>` tunes the persistent density source floor used to keep the source readable during GPU advection.
- Added `example-fluid-streamlines`, based on PixelFlow `Fluid_StreamLines` / `FlowField_LIC_StreamLines`. The example keeps the fluid solver on the GPU, reads back the velocity buffer, and renders bidirectional regular-grid CPU streamline segments over density/LIC views. Controls now cover the reference feature surface for pause, background mode, velocity vectors, seed particles, seed density, and line length.
- Added `ParticleFlow::spawn()` and `example-fluid-custom-particles`, based on PixelFlow `Fluid_CustomParticles`. The example keeps particle state on GPU texture path where available, respawns particles into local fluid sources, and maps left velocity+particles, middle heat+particles, and right particles-only input.
- Corrected a GPU spawn parity issue in `example-fluid-custom-particles`: same-frame source and mouse spawn requests were previously averaged into one center, causing visible mouse offset when the continuous source was active. GPU spawn requests now stay independent.

Review checklist:

- PixelFlow liquid-text source texture drives the fluid field: pass by screenshot inspection.
- PixelFlow liquid-painting source texture remains readable while being pulled into fluid trails: pass by user visual confirmation.
- PixelFlow streamline visual family is represented by regular seed-grid multi-segment lines driven by the current GPU velocity field, with streamlines promoted to the primary visual layer: pass by macOS build and code inspection.
- PixelFlow custom fluid-particle visual family is represented by local particle spawning into active fluid sources with GPU texture particles: pass by macOS build, screenshot inspection, and clean rerun logs after removing CPU seed texture upload.
- GPU fluid path remains active: pass by example HUD and `Fluid2D` GPU draw output.
- Tests still pass: pass.

Validation:

```bash
cmake -S addons/tcxFlowTools/examples/example-fluid-liquid-text -B addons/tcxFlowTools/examples/example-fluid-liquid-text/build-macos
cmake --build addons/tcxFlowTools/examples/example-fluid-liquid-text/build-macos --parallel 2
cmake -S addons/tcxFlowTools/examples/example-fluid-liquid-painting -B addons/tcxFlowTools/examples/example-fluid-liquid-painting/build-macos
cmake --build addons/tcxFlowTools/examples/example-fluid-liquid-painting/build-macos --parallel 2
cmake -S addons/tcxFlowTools/examples/example-fluid-streamlines -B addons/tcxFlowTools/examples/example-fluid-streamlines/build-macos
cmake --build addons/tcxFlowTools/examples/example-fluid-streamlines/build-macos --parallel 2
cmake -S addons/tcxFlowTools/examples/example-fluid-custom-particles -B addons/tcxFlowTools/examples/example-fluid-custom-particles/build-macos
cmake --build addons/tcxFlowTools/examples/example-fluid-custom-particles/build-macos --parallel 2
cmake --build addons/tcxFlowTools/tests/build-macos --parallel 2
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_softbody2d
```

All passed. Visual screenshot `/tmp/example-fluid-liquid-text-tuned.png` showed the generated text source preview and live density/temperature fluid response. Visual screenshot `/tmp/example-fluid-liquid-painting-v4.png` showed the Escher source staying readable while the right edge pulled into liquid-smoke trails; user confirmed the effect is correct. Synthetic macOS keypresses were blocked by accessibility permissions, so visual toggle verification used startup state and direct inspection rather than `osascript` key events.

Remaining gap: Phase 11 is still incomplete beyond the first liquid-text, liquid-painting, CPU-readback streamline, and custom fluid-particle examples. Verlet collision tuning, multiple fluids, velocity encoding, texture transfer, and exact GPU ping-pong/custom-render streamlines remain tracked parity targets.

## Phase 11B OpenProcessing/GPU-IO Inspired Physarum Trails

Reference source:

- Local source path: `/Users/mac/Downloads/sketch2174194`.
- OpenProcessing URL: `https://openprocessing.org/@u428391/2174194`.
- Source mechanism: GPU-IO velocity/pressure layers, three Jacobi pressure iterations, particle position/age layers, RK2 particle advection, fading trail texture, and render modes for Fluid, Pressure, and Velocity.

Implementation summary:

- Added `example-fluid-physarum-trails`.
- The implementation intentionally uses tcxFlowTools wrappers instead of copying the GPU-IO shader graph: `Fluid2D` supplies the low-resolution pressure-projected velocity field, then `PhysarumTrailFlow` keeps GPU particle position/age ping-pong state, samples velocity in shader, fades a trail FBO, and deposits short ink strokes through a GPU triangle batch.
- Mouse movement and drag are injected as segmented velocity splats to match the reference pointer-driven velocity path. The example exposes `flowRangeScale_` / `flowStrengthScale_` plus `TCX_PHYSARUM_FLOW_RANGE` and `TCX_PHYSARUM_FLOW_STRENGTH` so the splat range can be tuned without changing addon-level fluid defaults.
- The default mode uses a warm background, GPU trail FBO, and a uniform random particle field. It no longer enables autonomous vortices by default; the reference source is also pointer-driven. Keys `1`, `2`, and `3` switch Fluid, Pressure, and Velocity views; `+/-` changes trail length without rebuilding the system; `[` and `]` tune input range; `a`, `m`, `o`, `p`, `h`, and `r` toggle demo flow, move injection, particles, pause, HUD, and reset.
- The HUD prints FPS, frame milliseconds, particle count, path, batch count, and trail vertex count. The default path is `gpu-pingpong + gpu-trail`; CPU velocity readback and batched mesh deposition remain fallback/comparison only.

Implementation issues and final alignment:

- The first visual-parity path used CPU velocity readback and CPU short-stroke particles. It helped validate the reference character, but it ran too slowly for production-scale particle counts and could overflow sokol-gl immediate command capacity when drawing individual points.
- The accepted high-performance path moved particle position, age, velocity sampling, trail fade, and trail deposition to GPU passes. Particle age is stored in an `RGBA32F` ping-pong buffer rather than a single-channel float target to avoid backend-specific render-target support differences.
- Early parameter changes rebuilt the whole example and could make the HUD/path logic appear to fall back to CPU. Trail-length changes now update the live GPU trail setting, and the visible path is selected from GPU resource readiness rather than the previous frame's update flag.
- The final visual match came from preserving the reference invariants: 1/8-ish low-resolution velocity, max-clamped additive pointer splats, persistent pressure ping-pong, 1000-frame particle lifetime, three render substeps, fade-only trail accumulation, warm paper background, and dark blue ink. The main tuning variable was not shader complexity; it was constraining input range and strength so vortices remain readable and keep flowing instead of becoming whole-screen smears.

Review checklist:

- Core visual effect is represented through existing tcxFlowTools fluid/particle primitives: pass by code inspection and macOS build.
- Reference render modes are represented: pass by `ViewMode::Fluid`, `ViewMode::Pressure`, and `ViewMode::Velocity`.
- Exact GPU-IO layer naming/layout, CanvasCapture recording, PNG save behavior, and byte-for-byte shader behavior are intentionally not ported. The visual trail buffer is represented by a TrussC FBO and `PhysarumTrailFlow` GPU passes rather than GPU-IO `GPULayer` programs.
- Production-performance status: particle position/age ping-pong, shader-side velocity sampling, trail fade, and trail deposition are now GPU-side. Remaining performance/fidelity work is project-specific tuning for larger particle counts and cross-platform runtime validation beyond macOS/Metal.

Validation:

```bash
trusscli update -p /Users/mac/Desktop/TrussC/addons/tcxFlowTools/examples/example-fluid-physarum-trails
trusscli build -p /Users/mac/Desktop/TrussC/addons/tcxFlowTools/examples/example-fluid-physarum-trails
```

Both passed on macOS/Metal. The generated fallback `TRUSSC_DIR` path from `trusscli update` was restored to the addon-example relative path used by the existing examples.

Remaining queue after this effect:

1. PixelFlow Fluid/CFD: `VelocityEncoding`, `MultipleFluids`, `TexDataTransfer1/2/3`, custom-render streamlines, exact GPU ping-pong streamlines, `SlowBuoyancy`, and Verlet tuning.
2. PixelFlow OpticalFlow: capture/movie fluid, capture-driven Verlet particles, and PFM export.
3. PixelFlow FlowFieldParticles: cohesion, dam break, optical-flow capture, and sprite generator.
4. Reference-fidelity pass: geometry-shader/mesh visualizer details and pixel-level shader behavior after the missing functional queue is covered.

## Phase 12 Review

Implementation summary:

- Added an independent `tcx::flow::SoftBody2D` module inside tcxFlowTools. It is not backed by `tcxTraerPhysics`.
- The module currently supports Verlet particles, structural/shear/bend constraints, fixed particles, bounds, drag positioning, impulse/force input, nearest-particle lookup, and spring cutting.
- Added `example-softbody2d-cloth`, based on PixelFlow `SoftBody2D_Cloth`, with two cloth grids pinned at the top corners and rendered with translucent mesh fill, springs, particles, wind, mouse drag, and right-drag cutting.
- Added `tcxFlowTools_softbody2d` headless test coverage for grid construction, constraint solving, fixed particles, gravity, nearest lookup, and spring cutting.

Review checklist:

- SoftBody2D implementation is independent from `tcxTraerPhysics`: pass.
- PixelFlow `SoftBody2D_Cloth` core visual effect is represented: pass by user visual confirmation.
- Tests still pass: pass.
- TrussC core API changed: no.

Validation:

```bash
cmake -S addons/tcxFlowTools/examples/example-softbody2d-cloth -B addons/tcxFlowTools/examples/example-softbody2d-cloth/build-macos
cmake --build addons/tcxFlowTools/examples/example-softbody2d-cloth/build-macos --parallel 2
cmake --build addons/tcxFlowTools/tests/build-macos --parallel 2
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_softbody2d
```

All passed. Visual screenshot `/tmp/example-softbody2d-cloth.png` was reviewed, and the user confirmed the effect is correct.

Remaining gap: Phase 12 is still incomplete beyond the first SoftBody2D cloth example. SoftBody2D playground, chain, connected bodies, differential growth, liquid, particle collision, and SoftBody3D cloth/particle/playground examples remain tracked parity targets.
