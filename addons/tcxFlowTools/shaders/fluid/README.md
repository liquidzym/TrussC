# Fluid Shaders

`fluid.glsl` is compiled by sokol-shdc into `fluid.glsl.h` and provides the GPU-first `Fluid2D` graph:

- advect
- splat
- add velocity texture
- divergence
- Jacobi pressure solve
- gradient subtract / projection
- vorticity curl
- vorticity force
- buoyancy
- obstacle splat
- obstacle apply

CPU fluid code is fallback only for no-GPU/headless contexts.
