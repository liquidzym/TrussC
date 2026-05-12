# Particle Shaders

`particles.glsl` is compiled by sokol-shdc into `particles.glsl.h` and provides the default GPU particle path:

- spawn particle state into an RGBA32F texture
- update particle state from the GPU fluid velocity texture
- render helper pass
- point draw shader that samples the particle state texture in the vertex stage

`ParticleFlow` uses this GPU path by default. CPU particles are fallback only.
