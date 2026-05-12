# Portability Notes

Generated: 2026-05-10

## Current Pass

The current implementation avoids OpenGL-only APIs and does not change TrussC core. It uses TrussC public types (`Fbo`, `Texture`, `Vec2`, `Color`) plus CPU fallback arrays for the first visual smoke test.

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

## macOS / Metal

- Basic addon and CPU fallback build successfully in this checkout.
- The CPU density texture upload path uses TrussC `tc::Image` / `tc::TextureFilter::Linear` and does not require a core API addition.
- Real GPU shader passes still need sokol-shdc generated headers and Metal validation.

## Windows

- No Windows build has been run in this pass.
- Future shader work must validate HLSL uniform block layout and texture/sampler bindings.

## Linux

- No Linux build has been run in this pass.
- Future shader work must validate GL backend behavior and renderable float formats.

## Web / Emscripten

- No Web build has been run in this pass.
- `chooseRenderableFlowFormat()` currently returns `RGBA8` on Emscripten as a conservative fallback.
- Future implementation must detect or document half-float/float render-target support.

## Texture Origin And UVs

- Current CPU fallback drawing is screen-space and does not sample textures.
- Future shader passes must explicitly handle backend UV origin differences.

## Webcam Fallback

- Camera input is not implemented yet.
- `example-camera-fluid` currently uses procedural fallback input and should be replaced or extended with real camera/video texture input after bridge texture processing lands.
