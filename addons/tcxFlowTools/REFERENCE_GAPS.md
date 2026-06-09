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
- ofxFlowTools advanced bridge controls: bridge shader inputs support invert, alpha-mask use, mirror-X/Y, mask source selection, soft mask thresholding, and mask gamma; `example-fluid-bridges` exposes the controls.
- First-pass GPU LIC visualization: `Fluid2D::drawLic()` and `example-lic-streamlines` render a LIC-style texture over the GPU velocity field.
- ofxFlowTools field visualizers: `FlowVisualizer` supports styled velocity field arrows, velocity dots, pressure field, and temperature field modes; `example-wind-tunnel` exposes these views.
- GPU particles: `ParticleFlow` uses GPU state textures, spawn/update passes, and GPU drawing by default; CPU is fallback.
- First-pass particle variants: `ParticleFlowSettings::variant` and `example-particle-variants` cover flow, attractor, and impulse modes on the GPU path with CPU fallback parity.
- ofxFlowTools particle details: `ParticleFlowSettings` now exposes lifespan spread, mass, mass spread, size spread, age fade, and birth-from-velocity controls; GPU and CPU paths use mass in movement/draw and age/lifespan fade. GPU spawn initialization uses procedural shader seeds instead of a CPU seed texture upload.
- Local particle spawning: `ParticleFlow::spawn()` respawns GPU texture particles into local regions with CPU fallback parity. Same-frame spawn requests stay independent so continuous sources do not pull mouse spawns off target.
- ofxFlowTools average watcher: `AverageFlow` supports ROI sampling, normalized magnitude, velocity events, bounded history buffers, settings serialization, and `example-average-flow` overlays four watcher regions on a GPU fluid field.
- HD output-resolution split: `FluidSettings::outputResolutionScale`, `Fluid2D::outputWidth()/outputHeight()`, and `example-hd` cover separate simulation and GPU visualization/output resolution.
- PixelFlow-style wind tunnel direction: `example-wind-tunnel` exists for obstacle and texture-inlet validation.
- Fuller first-pass split-velocity shader graph: `SplitVelocity::updateTexture()` renders raw RGBA positive/negative velocity split, normalized split texture, decayed trail texture, combined/positive/negative/trail views, and split field overlay drawing in `example-split-velocity`.
- ofxFlowTools helper shader suite: extension fullscreen passes now cover colorize luminance, colorize velocity, colorize gradient, decay, dilate, erode, inverse warp, vector normalization, ease, and time blur; `FlowHelperPipeline` wraps these passes for high-level reusable pipelines.
- First-pass PixelFlow `Fluid_LiquidText` CFD example: `example-fluid-liquid-text` injects a generated text FBO into GPU density and temperature and disturbs it with fluid velocity.
- First-pass PixelFlow `Fluid_LiquidPainting` CFD example: `example-fluid-liquid-painting` injects the local PixelFlow Escher image into GPU density and uses procedural edge flow plus mouse drag to create liquid-smoke image smearing.
- First-pass PixelFlow streamline example: `example-fluid-streamlines` samples GPU fluid velocity through `Fluid2D::refreshVelocityReadback()` and renders bidirectional regular-grid CPU streamlines based on `Fluid_StreamLines` / `FlowField_LIC_StreamLines`, with controls for pause, background display mode, velocity vectors, seed particles, seed density, and line length.
- First-pass PixelFlow `Fluid_CustomParticles` example: `example-fluid-custom-particles` uses `ParticleFlow::spawn()` to inject GPU texture particles into fluid sources, with left velocity+particles, middle heat+particles, and right particles-only input.
- OpenProcessing/GPU-IO inspired Physarum trails example: `example-fluid-physarum-trails` references local source `/Users/mac/Downloads/sketch2174194` and preserves the core low-resolution velocity, pressure projection, particle aging, trail fade, and Fluid/Pressure/Velocity view character using tcxFlowTools `Fluid2D` plus GPU particle position/age ping-pong and a GPU trail-deposition pass.
- First-pass independent PixelFlow SoftBody2D foundation: `SoftBody2D` and `example-softbody2d-cloth` cover Verlet particles, structural/shear/bend constraints, fixed cloth anchors, wind, particle dragging, and spring cutting without depending on `tcxTraerPhysics`.
- Examples are centralized under `examples/`.

## Still Missing Or Partial

- ofxFlowTools bridge parity: deeper mask source, soft threshold, gamma, invert, alpha-mask, and mirror-X/Y controls exist. Exact ofx parameter surface and GUI binding remain partial.
- ofxFlowTools visualization parity: styled velocity dots/field and pressure/temperature field views exist. Exact reference geometry-shader rendering and richer GUI styling remain partial.
- ofxFlowTools extensions: `AverageFlowWatcher`-style history/settings support, fuller split-velocity GPU graph/field overlay, helper shader suite, and high-level helper-pipeline wrappers exist. Exact ofx GUI panels and mesh-rendering details remain partial.
- ofxFlowTools particle parity: age/lifespan/mass/draw/move controls and birth-from-velocity behavior have first-pass GPU and CPU support. Exact ofx/OpenGL particle shader byte-for-byte layout remains partial.
- ofxFlowTools HD parity: separate GPU visualization/output resolution exists; exact HD shader-family parity remains partial.
- OpenProcessing/GPU-IO Physarum parity: `example-fluid-physarum-trails` covers the core visual effect through tcxFlowTools abstractions and exposes FPS/frame/path/batch/vertex counters in the HUD. Exact GPU-IO `GPULayer` naming/layout, CanvasCapture recording, PNG save behavior, and byte-for-byte shader behavior are not ported. The current default path is GPU particle position/age ping-pong plus GPU trail fade/deposit; CPU velocity readback remains fallback/comparison only. Remaining fidelity work is final shader/colorize nuance and cross-platform runtime validation beyond macOS/Metal.
- PixelFlow flow-field visuals: first-pass LIC and CPU-readback streamlines are present; exact GPU ping-pong streamline particles, wind-tunnel LIC, image LIC, and optical-flow LIC examples are not ported.
- PixelFlow fluid examples: liquid text, liquid painting, CPU-readback streamlines, and custom fluid particles have first-pass parity examples; multiple fluids, custom render streamlines, Verlet/collision follow-up tuning, velocity encoding, and texture transfer examples remain candidates.
- PixelFlow optical-flow examples: movie/capture optical flow into fluid/particles and PFM export are not ported.
- PixelFlow flow-field particle variants: first-pass attractor and impulse modes exist; cohesion, dam break, sprite generator, and optical-flow capture particles are not ported.
- PixelFlow Softbody Dynamics is in scope: SoftBody2D cloth has a first-pass implementation; SoftBody2D chain, connected bodies, differential growth, liquid-like softbody behavior, particle collision, playground demos, and all SoftBody3D examples are not ported.
- PixelFlow Computational Fluid Dynamics examples are in scope: wind tunnel, liquid text, liquid painting, CPU-readback streamlines, and custom fluid particles have first-pass examples; exact GPU/custom-render streamlines, Verlet particle collision follow-up tuning, velocity encoding, multiple fluids, and texture transfer must still be covered by TrussC examples with matching core visual effects.
- PixelFlow Skylight, PostProcessing Filters, AntiAliasing, Shadertoy wrappers, sampling, and geometry/util modules are in scope as broader PixelFlow parity work. They can be implemented as staged tcxFlowTools modules or split into companion addons if the code boundary becomes cleaner, but they should not be treated as out-of-scope.

## Best Next Examples To Port

- `example-fluid-streamlines`: first-pass CPU-readback streamline particles exist; next step is exact PixelFlow GPU ping-pong/custom-render streamline parity if performance or visual fidelity requires it.
- `example-wind-tunnel`: based on PixelFlow `Fluid_WindTunnel` / `Fluid_WindTunnel_LIC`, useful for obstacle validation.
- `example-movie-fluid`: based on PixelFlow `OpticalFlow_MovieFluid`, using TrussC `VideoPlayer` texture input.
- `example-particle-variants`: first-pass attractor and impulse modes exist; next useful step is cohesion or optical-flow capture particle behavior.
- `example-split-velocity`: based on ofxFlowTools split-velocity shaders and visualizers.
- `example-softbody2d-playground`: next SoftBody2D parity target after the first-pass cloth example, based on PixelFlow `SoftBody2D_Playground`.
- `example-fluid-verlet-collision`: CFD/particle collision parity target, based on PixelFlow `Fluid_VerletParticleCollisionSystem`.
- `example-skylight-basic`: first broader PixelFlow renderer target, based on PixelFlow `Skylight_Basic`.

## Priority Backlog Added 2026-06-08

The next parity work should proceed in this order unless a build/runtime blocker is found:

1. ofxFlowTools bridge/visualizer/split/helper/particle deep pass: completed to first-pass TrussC parity on 2026-06-08, with exact reference GUI/geometry styling still open for later visual tuning.
2. PixelFlow Fluid/CFD: `Fluid_VelocityEncoding`, `Fluid_MultipleFluids`, `Fluid_GetStarted_TexDataTransfer1/2/3`, `Fluid_StreamLines_CustomRender`, more exact GPU ping-pong streamlines, `Fluid_SlowBuoyancy`, and follow-up tuning for `Fluid_VerletParticleCollisionSystem`.
3. PixelFlow OpticalFlow: capture/movie fluid examples, capture-driven Verlet particles, and PFM export.
4. PixelFlow FlowFieldParticles: cohesion, dam break, optical-flow capture particles, and sprite generator examples.
5. Reference-fidelity tuning: ofxFlowTools geometry-shader/mesh details and pixel-level shader behavior should be revisited only after the missing PixelFlow queue is covered, because the TrussC/sokol graph intentionally differs from the original OpenGL/Processing implementations.

No TrussC core API changes were made for this audit.
