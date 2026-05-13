# Reference Gaps

Generated: 2026-05-10

This is the current audit against the checked reference repositories in `_fcache/ofxFlowTools-master`, `_fcache/ofxFlowTools-HD`, and `_fcache/PixelFlow`.

Visual parity target:

- ofxFlowTools reference video: `https://vimeo.com/92334462`. The target is the live-camera/optical-flow/fluid shader character, not just matching API names.
- PixelFlow reference index: `https://vimeo.com/diwi` plus the local `_fcache/PixelFlow/README.md` video/example categories. The target includes Fluid Simulation, Flow Field Particles, Softbody Dynamics, Computational Fluid Dynamics, Skylight, and PostProcessing families.
- Exact one-to-one ports are not required for every demo, but each implemented TrussC example must preserve the core visual effect of its reference family.

## Completed In tcxFlowTools

- GPU-first `Fluid2D`: advection, splats, velocity texture injection, vorticity, buoyancy, divergence, Jacobi pressure solve, projection, and debug rendering.
- GPU obstacle mask API: `addObstacle()` and `clearObstacles()` with CPU fallback.
- GPU texture optical flow: `OpticalFlow::update(const tc::Texture&, float)` and GPU flow/current/previous texture getters.
- Real camera example: `example-camera-fluid` connects `tc::VideoGrabber` texture directly into GPU optical flow and GPU fluid.
- GPU external texture bridges: `VelocityBridge`, `DensityBridge`, `TemperatureBridge`, and `CombinedBridge` render texture outputs and apply them to `Fluid2D`; `example-fluid-bridges` visually verifies modes 1-4.
- First-pass GPU LIC visualization: `Fluid2D::drawLic()` and `example-lic-streamlines` render a LIC-style texture over the GPU velocity field.
- GPU particles: `ParticleFlow` uses GPU state textures, spawn/update passes, and GPU drawing by default; CPU is fallback.
- First-pass particle variants: `ParticleFlowSettings::variant` and `example-particle-variants` cover flow, attractor, and impulse modes on the GPU path with CPU fallback parity.
- HD output-resolution split: `FluidSettings::outputResolutionScale`, `Fluid2D::outputWidth()/outputHeight()`, and `example-hd` cover separate simulation and GPU visualization/output resolution.
- PixelFlow-style wind tunnel direction: `example-wind-tunnel` exists for obstacle and texture-inlet validation.
- First-pass split-velocity shader output: `SplitVelocity::updateTexture()` and `example-split-velocity` render combined/positive/negative GPU velocity-channel views.
- First-pass PixelFlow `Fluid_LiquidText` CFD example: `example-fluid-liquid-text` injects a generated text FBO into GPU density and temperature and disturbs it with fluid velocity.
- First-pass PixelFlow `Fluid_LiquidPainting` CFD example: `example-fluid-liquid-painting` injects the local PixelFlow Escher image into GPU density and uses procedural edge flow plus mouse drag to create liquid-smoke image smearing.
- First-pass independent PixelFlow SoftBody2D foundation: `SoftBody2D` and `example-softbody2d-cloth` cover Verlet particles, structural/shear/bend constraints, fixed cloth anchors, wind, particle dragging, and spring cutting without depending on `tcxTraerPhysics`.
- Examples are centralized under `examples/`.

## Still Missing Or Partial

- ofxFlowTools bridge parity: advanced controls such as invert, alpha-mask, mirror axes, and deeper mask options are still partial.
- ofxFlowTools visualization parity: velocity dots/field classes, pressure/temperature field styling, and watcher-style UI examples are not fully ported.
- ofxFlowTools extensions: `AverageFlowWatcher`, full split-velocity shader graph, colorize luminance/velocity/gradient, decay, dilate, erode, inverse warp, normalization, ease, and time blur helper shaders are not fully ported. A first split-velocity GPU visual pass exists.
- ofxFlowTools particle parity: age/lifespan/mass tuning and richer draw/move shader controls are only represented by the first GPU particle path plus simple size/color controls.
- ofxFlowTools HD parity: separate GPU visualization/output resolution exists; exact HD shader-family parity remains partial.
- PixelFlow flow-field visuals: first-pass LIC is present; full streamline particle rendering, wind-tunnel LIC, image LIC, and optical-flow LIC examples are not ported.
- PixelFlow fluid examples: liquid text and liquid painting have first-pass parity examples; multiple fluids, custom render streamlines, Verlet/collision demos, velocity encoding, and texture transfer examples remain candidates.
- PixelFlow optical-flow examples: movie/capture optical flow into fluid/particles and PFM export are not ported.
- PixelFlow flow-field particle variants: first-pass attractor and impulse modes exist; cohesion, dam break, sprite generator, and optical-flow capture particles are not ported.
- PixelFlow Softbody Dynamics is in scope: SoftBody2D cloth has a first-pass implementation; SoftBody2D chain, connected bodies, differential growth, liquid-like softbody behavior, particle collision, playground demos, and all SoftBody3D examples are not ported.
- PixelFlow Computational Fluid Dynamics examples are in scope: wind tunnel, liquid text, and liquid painting have first-pass examples; streamlines, Verlet particle collision system, fluid particles, velocity encoding, multiple fluids, and texture transfer must still be covered by TrussC examples with matching core visual effects.
- PixelFlow Skylight, PostProcessing Filters, AntiAliasing, Shadertoy wrappers, sampling, and geometry/util modules are in scope as broader PixelFlow parity work. They can be implemented as staged tcxFlowTools modules or split into companion addons if the code boundary becomes cleaner, but they should not be treated as out-of-scope.

## Best Next Examples To Port

- `example-lic-streamlines`: first-pass LIC exists; next step is PixelFlow-style streamline particle rendering based on `FlowField_LIC_StreamLines`.
- `example-wind-tunnel`: based on PixelFlow `Fluid_WindTunnel` / `Fluid_WindTunnel_LIC`, useful for obstacle validation.
- `example-movie-fluid`: based on PixelFlow `OpticalFlow_MovieFluid`, using TrussC `VideoPlayer` texture input.
- `example-particle-variants`: first-pass attractor and impulse modes exist; next useful step is cohesion or optical-flow capture particle behavior.
- `example-split-velocity`: based on ofxFlowTools split-velocity shaders and visualizers.
- `example-softbody2d-playground`: next SoftBody2D parity target after the first-pass cloth example, based on PixelFlow `SoftBody2D_Playground`.
- `example-fluid-verlet-collision`: CFD/particle collision parity target, based on PixelFlow `Fluid_VerletParticleCollisionSystem`.
- `example-skylight-basic`: first broader PixelFlow renderer target, based on PixelFlow `Skylight_Basic`.

No TrussC core API changes were made for this audit.
