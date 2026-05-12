# Migration Report

Generated: 2026-05-09

Reference checkouts used for this pass:

- ofxFlowTools `master`: `17cabe2`
- ofxFlowTools `HD`: `8754377`
- PixelFlow `master`: `3db4939`

Current implementation status: Phases 1-9 have buildable coverage and strict audit entries in `PHASE_GATES.md`. The addon builds as a TrussC addon, centralizes examples under `examples/`, and now uses GPU-first fluid, optical-flow texture input, obstacle masking, and particle paths with CPU fallback only for no-GPU/headless contexts.

| Source | Original module / class | New module / class | Status | Notes |
|---|---|---|---|---|
| ofxFlowTools master / HD | `src/core/ftPingPongFbo.h` | `tcxFlow/Core/PingPongBuffer` | rewritten scaffold | Uses TrussC `Fbo`, no OpenGL handles exposed. |
| ofxFlowTools master / HD | `src/core/fluid/ftFluidFlow.*` | `tcxFlow/Fluid/Fluid2D` | GPU rewrite | Settings, resize, injection, GPU ping-pong buffers, advection, vorticity, buoyancy, obstacle mask, divergence/Jacobi/projection, and GPU debug display are present; separate density/output resolution is pending. |
| ofxFlowTools HD | `src/core/fluid/ftSimpleFluidFlow.h` | `Fluid2D` simplified path | referenced | Useful for a smaller starter example; not directly copied. |
| ofxFlowTools master / HD | `src/core/opticalflow/ftOpticalFlow.h` | `tcxFlow/OpticalFlow/OpticalFlow` | GPU texture rewrite | Procedural moving luminance frames remain for tests/fallback; real camera/video/texture input now runs through GPU current/previous luminance FBOs and exposes a GPU flow texture. |
| ofxFlowTools master / HD | bridge classes | `BridgeFlow`, `VelocityBridge`, `DensityBridge`, `TemperatureBridge`, `CombinedBridge`, `shaders/bridge/bridge.glsl` | partial rewrite | Hierarchy and generated bridge shader assets exist; velocity texture graph is wired through `Fluid2D::applyVelocityTexture()`, while full density/temperature external texture wiring is pending. |
| ofxFlowTools HD | `ftColorBridgeFlow.h` | future color bridge or `DensityBridge` extension | referenced | Not implemented yet. |
| ofxFlowTools master / HD | visualization classes | `FlowVisualizer`, `shaders/visualization/visualization.glsl` | partial rewrite | GPU debug visualizers are used for GPU fluid views; CPU drawing remains as fallback. |
| ofxFlowTools extensions | mouse / average / particles / splitvelocity | `MouseFlow`, `AverageFlow`, `SplitVelocity`, `ParticleFlow`, `shaders/particles/particles.glsl` | GPU-first rewrite | Mouse input drives GPU fluid by default; particles use GPU state/update/draw by default, while CPU analytics and CPU particles remain fallback/extension paths. |
| PixelFlow | `DwFluid2D.java` | `Fluid2D` GPU solver | referenced | Pressure, velocity, density pipeline structure is carried into the TrussC/sokol GPU graph. |
| PixelFlow | filters | future `FlowPass` shader utilities | referenced | Copy, blur, difference, luminance, threshold map to common shader passes. |
| PixelFlow / ofxFlowTools | optical-flow shaders | `OpticalFlow`, `shaders/opticalflow/opticalflow.glsl` | partial rewrite | Procedural moving luminance frames now produce CPU flow; generated optical-flow shader passes are present. |
| PixelFlow | `DwFlowFieldParticles.java` | `ParticleFlow` | first GPU rewrite | GPU state texture, update pass, and particle draw path are implemented; advanced particle variants remain tracked in `REFERENCE_GAPS.md`. |

## File-Level Notes

Original source file: `ofxFlowTools/src/core/ftPingPongFbo.h`
Original branch: `master` / `HD`
New file: `src/tcxFlow/Core/PingPongBuffer.*`
Status: rewritten
Reason: TrussC must not expose or depend on OpenGL-only FBO handles.
License notes: no code copied.

Original source file: `ofxFlowTools/src/core/fluid/ftFluidFlow.*`
Original branch: `master` / `HD`
New file: `src/tcxFlow/Fluid/Fluid2D.*`, `src/tcxFlow/Fluid/FluidBuffers.*`, `shaders/fluid/fluid.glsl`
Status: GPU rewrite
Reason: public API, resource lifecycle, generated fluid shader pass assets, obstacle masks, and the default GPU update graph are in place; CPU remains a fallback for no graphics context.
License notes: no code copied.

Original source file: `PixelFlow/src/com/thomasdiewald/pixelflow/java/fluid/DwFluid2D.java`
Original branch: `master`
New file: `shaders/fluid/fluid.glsl`
Status: referenced and rewritten
Reason: Java/Processing implementation cannot be directly used in TrussC; the shader pass structure and pressure-projection sign convention are carried over as a TrussC/sokol shader set.
License notes: no code copied.

Original source file: `ofxFlowTools/src/core/opticalflow/ftOpticalFlowShader.h` and `PixelFlow/src/com/thomasdiewald/pixelflow/glsl/OpticalFlow/*`
Original branch: `master`
New file: `src/tcxFlow/OpticalFlow/OpticalFlow.*`, `shaders/opticalflow/opticalflow.glsl`
Status: partial rewrite
Reason: current/previous luminance tracking and gradient/time-difference flow estimation were adapted to TrussC; real `tc::Texture` inputs now stay on GPU through luminance, optical-flow, and temporal-smooth passes.
License notes: no code copied.

Original source file: `ofxFlowTools` bridge, visualization, and extension modules plus PixelFlow filter/particle shader references
Original branch: `master` / `HD`
New file: `src/tcxFlow/Bridge/*`, `src/tcxFlow/Visualization/*`, `src/tcxFlow/Extensions/*`, `shaders/bridge/bridge.glsl`, `shaders/visualization/visualization.glsl`, `shaders/particles/particles.glsl`
Status: partial rewrite
Reason: module APIs and generated shader pass assets are now present; fluid, camera/video optical flow, and particles are GPU-first, while full bridge parity and advanced particle examples remain future work.
License notes: no code copied.

## Tests / Examples

- `tests/test_settings.cpp` covers default settings, resize, density/velocity/temperature injection, pressure generation, finite energy, procedural optical-flow frame/flow generation, and flow injection.
- `tests/test_core_contracts.cpp` covers move-only resource contracts, FluidBuffers ownership contracts, generated shader header presence for common/fluid/opticalflow/bridge/visualization/particles shaders, and FlowPass readiness contracts.
- `examples/example-simple`, `examples/example-core-pingpong`, `examples/example-optical-flow`, `examples/example-fluid-bridges`, `examples/example-camera-fluid`, `examples/example-particles`, and `examples/example-hd` are visual smoke tests under `examples/`; app fluid examples use GPU fluid by default.
- `example-camera-fluid` now validates the real camera texture path: `VideoGrabber::getTexture()` -> `OpticalFlow::update(texture)` -> `OpticalFlow::getFlowTexture()` -> `Fluid2D::applyVelocityTexture()`.
- `example-particles` now validates GPU particle state/update/draw over the GPU fluid velocity texture.
- `examples/example-simple` was visually corrected on 2026-05-10: density display now uses a linear-filtered dynamic texture instead of enlarged simulation-cell rectangles.
- A second `examples/example-simple` correction on 2026-05-10 aligned pressure projection and vorticity formulas with the ofxFlowTools/PixelFlow shader pipeline, changed mouse input to small continuous impulses, and moved the default update path to GPU.
