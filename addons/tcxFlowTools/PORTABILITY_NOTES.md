# Portability Notes

Generated: 2026-05-10
Reviewed: 2026-06-08

## Current Pass

The current implementation avoids OpenGL-only APIs and does not change TrussC core. It uses TrussC public types (`Fbo`, `Texture`, `Vec2`, `Color`) plus CPU fallback arrays.

Shader sources are generated through sokol-shdc for `metal_macos:hlsl5:glsl300es:wgsl`. On 2026-06-08, all addon shader groups (`common`, `fluid`, `opticalflow`, `bridge`, `visualization`, `particles`, and `extensions`) were re-generated to `/tmp` with that target set for a source-level cross-backend check.

Verified on 2026-05-09:

- macOS configure/build for `tests`.
- macOS configure/build for `example-simple`.
- `tcxFlowTools_settings` test executable ran successfully.

Verified on 2026-05-10:

- macOS configure/build for `example-core-pingpong`.
- macOS build regression for `example-simple`.
- `tcxFlowTools_settings` and `tcxFlowTools_core_contracts` test executables ran successfully.
- CPU fallback `Fluid2D` solver path produced finite density/velocity/pressure/temperature state in tests.
- All examples are now centralized under `addons/tcxFlowTools/examples/`.
- `example-simple`, `example-core-pingpong`, `example-optical-flow`, `example-fluid-bridges`, `example-camera-fluid`, `example-particles`, and `example-hd` configured and built on macOS from the centralized examples folder.
- `example-simple` density display was corrected from per-cell rectangle drawing to dynamic linear-filtered texture drawing.

Verified on 2026-06-08:

- macOS/Metal build for `tests`.
- `tcxFlowTools_settings`, `tcxFlowTools_core_contracts`, and `tcxFlowTools_softbody2d` ran successfully.
- macOS/Metal build for `example-fluid-verlet-collision`.
- macOS/Metal configure/build for `example-fluid-streamlines`.
- macOS/Metal configure/build for `example-fluid-custom-particles`.
- macOS/Metal configure/build for `example-average-flow`.
- macOS/Metal configure/build for `example-fluid-liquid-painting` after the user reported it had not compiled earlier.
- macOS/Metal configure/build for `example-softbody2d-cloth` after the user reported it had not compiled earlier.
- macOS/Metal configure/build for `example-split-velocity` after adding the fuller split-velocity graph.
- macOS/Metal configure/build for every current `examples/example-*` app; all 18 examples passed.
- macOS/Metal rebuild for every current `examples/example-*` app after adding `FlowHelperPipeline`, deeper bridge masks, styled field drawing, AverageFlow history/settings support, split field overlay, and particle birth-from-velocity controls; all 18 examples passed.
- `example-simple` was rebuilt after fixing the newly exposed enum coverage warning from expanded visualizer modes.
- Shader source generation for `metal_macos:hlsl5:glsl300es:wgsl` succeeded for all addon shader groups.
- GUI screenshot review covered every current example. The review did not find black screens, stale texture bindings, or obvious wrong-output defaults in the sampled startup states.
- GPU particle spawn initialization now uses shader-side procedural seeds instead of a same-frame CPU seed texture upload. `example-particles`, `example-particle-variants`, and `example-fluid-custom-particles` were rerun after the fix and produced empty logs instead of the previous `Texture::loadData()` warning.

## macOS / Metal

- Addon, CPU fallback, GPU fluid passes, generated shader headers, and all current example apps build successfully in this checkout.
- Bridge mask source/softness/gamma and particle birth/layout shader changes generated successfully through `sokol-shdc` for `metal_macos:hlsl5:glsl300es:wgsl`; no backend-specific shader syntax was introduced.
- The CPU density texture upload path uses TrussC `tc::Image` / `tc::TextureFilter::Linear` and does not require a core API addition.
- `Fluid2D::refreshVelocityReadback()` uses TrussC `Fbo::readPixelsFloat()` on the current GPU velocity buffer so CPU particle examples can sample real GPU fluid velocity.
- `ParticleFlow` GPU spawn does not require a CPU seed texture upload, avoiding TrussC's same-frame dynamic texture update guard on all backends.
- `FlowVisualizer` and `SplitVelocity::drawField()` use TrussC immediate 2D drawing instead of geometry shaders, so the first-pass field styling avoids OpenGL-only geometry-stage dependencies.

## Windows

- No Windows build has been run in this pass.
- Shader source generation includes `hlsl5`, but HLSL runtime behavior, uniform block layout, texture/sampler bindings, and GPU velocity readback still need a Windows build/run pass.

## Linux

- No Linux build has been run in this pass.
- Shader source generation includes `glsl300es`, but GL runtime behavior, renderable float formats, and GPU velocity readback still need a Linux build/run pass.

## Web / Emscripten

- No Web build has been run in this pass.
- `chooseRenderableFlowFormat()` currently returns `RGBA8` on Emscripten as a conservative fallback.
- WebGPU does not currently support synchronous `Fbo::readPixelsFloat()` in TrussC, so `Fluid2D::refreshVelocityReadback()` returns `false` on Emscripten GPU updates instead of logging every frame.
- `example-fluid-streamlines` depends on `Fluid2D::refreshVelocityReadback()` for its CPU streamline overlay; on Web it should still show the fluid/LIC views but the streamline overlay is expected to stay disabled until async readback or a GPU streamline path exists.
- Future implementation must detect or document half-float/float render-target support and async velocity readback.

## Texture Origin And UVs

- Current CPU fallback drawing is screen-space and does not sample textures.
- Future shader passes must explicitly handle backend UV origin differences.

## Webcam Fallback

- `example-camera-fluid` uses real `tc::VideoGrabber` texture input when camera permission/input is available.
- Procedural fallback input remains for no-camera or permission-denied runs.
