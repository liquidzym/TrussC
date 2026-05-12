# Known Limitations

Generated: 2026-05-10

- `Fluid2D` defaults to the GPU solver path. CPU simulation exists only as a fallback when no valid sokol graphics context is available, headless mode is active, or `FluidSettings::useGpu` is disabled.
- The GPU fluid path currently covers splat, velocity texture injection, advection, optional vorticity, optional buoyancy, obstacle masks, divergence, Jacobi pressure solve, projection, and GPU density/velocity/pressure/temperature display.
- `OpticalFlow` supports GPU texture input through `update(const tc::Texture&, float)`. Tests still exercise procedural CPU flow because they run without an app graphics context.
- `example-camera-fluid` now uses real `tc::VideoGrabber` texture input directly into GPU optical flow, with procedural fallback only when camera/permission/GPU input is unavailable.
- Bridge classes provide procedural injection APIs and generated bridge shader assets; full external texture-to-density/temperature bridge wiring is still pending beyond velocity texture injection.
- `ParticleFlow` defaults to GPU state textures, a GPU update pass, and GPU particle drawing. CPU particles remain fallback when GPU is unavailable.
- GPU particles are intentionally a first complete TrussC/sokol implementation, but they do not yet cover every PixelFlow/ofxFlowTools particle variant such as collision, attractors/cohesion, sprite trails, and dam-break style examples.
- ofxFlowTools HD separates simulation resolution from density/output resolution more deeply than the current single-grid `Fluid2D`; exact HD visual parity is still pending.
- All requested example directories are centralized under `examples/` and build on macOS. Current app examples use GPU fluid by default; tests intentionally exercise CPU fallback because they run without an app graphics context.
- LIC/streamlines, velocity-dot/field visualizers, average watchers, split-velocity shader parity, colorize/erode/dilate/inverse-warp/timeblur helper shaders, and PixelFlow wind-tunnel/multiple-fluid examples are still tracked in `REFERENCE_GAPS.md`.
- Only the current macOS checkout is being tested in this pass. Windows, Linux, Web, iOS, and Android remain unverified.
- No TrussC core API was modified in this pass.
