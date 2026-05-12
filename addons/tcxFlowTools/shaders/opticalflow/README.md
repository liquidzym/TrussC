# Optical Flow Shaders

`opticalflow.glsl` is compiled by sokol-shdc into `opticalflow.glsl.h` and provides GPU current/previous texture optical flow:

- luminance preprocess
- difference
- gradient
- optical flow
- temporal smooth
- visualize

`OpticalFlow::update(const tc::Texture&, float)` uses these passes for real camera, video, or any TrussC texture input. Procedural CPU flow remains for tests and fallback only.
