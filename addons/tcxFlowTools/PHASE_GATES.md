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
| Phase 10 | ofx helper shader gaps | split velocity / average watcher / helper filters | In progress |
| Phase 11 | PixelFlow CFD + flow-field examples | liquid text/painting, streamlines, collision, velocity encoding | Planned |
| Phase 12 | PixelFlow Softbody Dynamics | SoftBody2D/3D cloth, chains, collision, playground | Planned |
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
- Advanced bridge settings such as invert, alpha-mask, and mirror controls remain pending and are tracked in `REFERENCE_GAPS.md`.
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
- Remaining parity gap: advanced bridge controls such as invert, alpha-mask, mirror axes, and deeper mask options are not yet implemented.

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

Phase 9 is complete for the current GPU-first milestone. CPU remains a no-GPU fallback. Remaining reference gaps now include deeper bridge parity, full streamline particle rendering, exact HD shader-family parity beyond output resolution, richer particle variants, helper shader parity, broader PixelFlow CFD examples, PixelFlow Softbody Dynamics, Skylight/PostProcessing/AntiAliasing/Shadertoy/sampling/geometry modules, and cross-platform builds; see `REFERENCE_GAPS.md`.

## Phase 10 Review

Implementation summary:

- `SplitVelocity` still computes CPU sampled positive/negative velocity metrics.
- `SplitVelocity::updateTexture()` adds a first GPU helper shader output for combined, positive, and negative velocity channels.
- `shaders/extensions/extensions.glsl` adds the generated split-velocity helper pass.
- `FlowPassKind::ExtensionSplitVelocity` maps the generated pass through the shared fullscreen pass wrapper.
- `example-split-velocity` demonstrates combined/positive/negative split views over GPU fluid.

Review checklist:

- Split-velocity helper shader compiles for `metal_macos:hlsl5:glsl300es:wgsl`: pass by build.
- Generated extension shader header exists and pass mapping is reachable through `FlowPassKind`: pass by `test_core_contracts`.
- Existing CPU split metrics still run: pass by `tcxFlowTools_settings`.
- Visual example builds: pass on macOS.
- Visual example renders nonblank combined/positive/negative GPU views: pass by screenshot audit.
- Full ofx helper parity remains partial: velocity-dot/field visualizers, AverageFlowWatcher, colorize helpers, decay, dilate/erode, inverse warp, normalization, ease, and time blur are not done.
- TrussC core API changed: no.

Review commands:

```bash
cmake -S addons/tcxFlowTools/examples/example-split-velocity -B addons/tcxFlowTools/examples/example-split-velocity/build-macos
cmake --build addons/tcxFlowTools/examples/example-split-velocity/build-macos --parallel 2
cmake --build addons/tcxFlowTools/tests/build-macos --parallel 2
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_settings
addons/tcxFlowTools/tests/build-macos/tcxFlowTools_core_contracts
```

## Phase 11+ Scope Note

2026-05-13 clarification:

- The visual references are the ofxFlowTools Vimeo example and PixelFlow's Vimeo/example index. The goal is matching the core visual effect family, not only compiling similarly named APIs.
- PixelFlow Softbody Dynamics and Computational Fluid Dynamics examples are explicitly in scope.
- Broader PixelFlow modules such as Skylight, PostProcessing Filters, AntiAliasing, Shadertoy wrappers, sampling, and geometry/util examples are also in parity scope unless a later task splits them into companion addons.
