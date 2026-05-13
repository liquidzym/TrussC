# Known Limitations

Generated: 2026-05-10

- `Fluid2D` defaults to the GPU solver path. CPU simulation exists only as a fallback when no valid sokol graphics context is available, headless mode is active, or `FluidSettings::useGpu` is disabled.
- The GPU fluid path currently covers splat, velocity/density/temperature texture injection, advection, optional vorticity, optional buoyancy, obstacle masks, divergence, Jacobi pressure solve, projection, and GPU density/velocity/pressure/temperature/combined/LIC display.
- `OpticalFlow` supports GPU texture input through `update(const tc::Texture&, float)`. Tests still exercise procedural CPU flow because they run without an app graphics context.
- `example-camera-fluid` now uses real `tc::VideoGrabber` texture input directly into GPU optical flow, with procedural fallback only when camera/permission/GPU input is unavailable.
- Bridge classes provide procedural fallback injection APIs, generated bridge shader assets, and GPU external texture wiring for velocity, density, temperature, and combined bridge injection.
- Advanced bridge controls such as `BridgeSettings::invert`, `useAlphaAsMask`, `mirrorX`, and `mirrorY` remain tracked parity gaps.
- `ParticleFlow` defaults to GPU state textures, a GPU update pass, and GPU particle drawing. CPU particles remain fallback when GPU is unavailable. First-pass flow, attractor, and impulse variants are available.
- GPU particles are intentionally a first complete TrussC/sokol implementation, but they do not yet cover every PixelFlow/ofxFlowTools particle variant such as collision, cohesion, sprite trails, optical-flow capture particles, and dam-break style examples.
- `Fluid2D` can separate simulation resolution from GPU visualization/output resolution through `FluidSettings::outputResolutionScale`. The simulation itself still uses a single velocity/density/temperature grid, so exact ofxFlowTools HD shader-family parity is still pending.
- All requested example directories are centralized under `examples/` and build on macOS. Current app examples use GPU fluid by default; tests intentionally exercise CPU fallback because they run without an app graphics context.
- Full streamline particle rendering, velocity-dot/field visualizers, average watchers, full split-velocity shader parity, colorize/erode/dilate/inverse-warp/timeblur helper shaders, and PixelFlow multiple-fluid examples are still tracked in `REFERENCE_GAPS.md`.
- PixelFlow `Fluid_LiquidText` and `Fluid_LiquidPainting` now have first-pass CFD parity examples, but broader Computational Fluid Dynamics examples such as Verlet particle collision, velocity encoding, multiple fluids, texture transfer, and streamlines remain partial or missing.
- PixelFlow SoftBody2D cloth now has an independent first-pass implementation in tcxFlowTools. SoftBody2D playground, chain, connected bodies, differential growth, liquid/collision systems, and SoftBody3D remain missing.
- PixelFlow Skylight, post-processing, anti-aliasing, Shadertoy-style wrappers, sampling, and geometry/util families are explicit parity scope after the 2026-05-13 reference clarification, but they are not yet implemented.
- Only the current macOS checkout is being tested in this pass. Windows, Linux, Web, iOS, and Android remain unverified.
- No TrussC core API was modified in this pass.
