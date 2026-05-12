# Reference Gaps

Generated: 2026-05-10

This is the current audit against the checked reference repositories in `_fcache/ofxFlowTools-master`, `_fcache/ofxFlowTools-HD`, and `_fcache/PixelFlow`.

## Completed In tcxFlowTools

- GPU-first `Fluid2D`: advection, splats, velocity texture injection, vorticity, buoyancy, divergence, Jacobi pressure solve, projection, and debug rendering.
- GPU obstacle mask API: `addObstacle()` and `clearObstacles()` with CPU fallback.
- GPU texture optical flow: `OpticalFlow::update(const tc::Texture&, float)` and GPU flow/current/previous texture getters.
- Real camera example: `example-camera-fluid` connects `tc::VideoGrabber` texture directly into GPU optical flow and GPU fluid.
- GPU external texture bridges: `VelocityBridge`, `DensityBridge`, `TemperatureBridge`, and `CombinedBridge` render texture outputs and apply them to `Fluid2D`; `example-fluid-bridges` visually verifies modes 1-4.
- GPU particles: `ParticleFlow` uses GPU state textures, spawn/update passes, and GPU drawing by default; CPU is fallback.
- PixelFlow-style wind tunnel direction: `example-wind-tunnel` exists for obstacle and texture-inlet validation.
- Examples are centralized under `examples/`.

## Still Missing Or Partial

- ofxFlowTools bridge parity: advanced controls such as invert, alpha-mask, mirror axes, and deeper mask options are still partial.
- ofxFlowTools visualization parity: velocity dots/field classes, pressure/temperature field styling, and watcher-style UI examples are not fully ported.
- ofxFlowTools extensions: `AverageFlowWatcher`, full split-velocity shader graph, colorize luminance/velocity/gradient, decay, dilate, erode, inverse warp, normalization, ease, and time blur helper shaders are not fully ported.
- ofxFlowTools particle parity: age/lifespan/mass/size tuning and richer draw/move shader controls are only represented by the first GPU particle path.
- ofxFlowTools HD parity: separate density/output resolution and exact HD visual pipeline remain partial.
- PixelFlow flow-field visuals: LIC, streamlines, wind-tunnel LIC, image LIC, and optical-flow LIC examples are not ported.
- PixelFlow fluid examples: multiple fluids, liquid painting/text, custom render streamlines, Verlet/collision demos, velocity encoding, and texture transfer examples remain candidates.
- PixelFlow optical-flow examples: movie/capture optical flow into fluid/particles and PFM export are not ported.
- PixelFlow flow-field particle variants: attractors, cohesion, impulse, dam break, sprite generator, and optical-flow capture particles are not ported.
- PixelFlow non-flow modules such as skylight, soft bodies, broad image-processing filters, Shadertoy demos, anti-aliasing, and miscellaneous geometry are outside the current addon scope unless explicitly requested.

## Best Next Examples To Port

- `example-lic-streamlines`: based on PixelFlow `FlowField_LIC_StreamLines` and `Fluid_Basic_LIC`.
- `example-wind-tunnel`: based on PixelFlow `Fluid_WindTunnel` / `Fluid_WindTunnel_LIC`, useful for obstacle validation.
- `example-movie-fluid`: based on PixelFlow `OpticalFlow_MovieFluid`, using TrussC `VideoPlayer` texture input.
- `example-particle-variants`: based on PixelFlow `FlowFieldParticles_Attractors`, `Impulse`, and `Cohesion`.
- `example-split-velocity`: based on ofxFlowTools split-velocity shaders and visualizers.

No TrussC core API changes were made for this audit.
