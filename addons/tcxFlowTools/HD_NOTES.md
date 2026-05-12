# HD Notes

Generated: 2026-05-10

Inspected:

- ofxFlowTools `master`: `17cabe2`
- ofxFlowTools `HD`: `8754377`

## Master vs HD Summary

| Area | HD difference observed | tcxFlowTools action |
|---|---|---|
| Examples | `example_simple` exists only on HD. Core and extended examples differ. | Added `examples/example-simple` as the first smoke test and centralized all examples under `examples/`. |
| Fluid | `ftSimpleFluidFlow.h` exists only on HD. `ftFluidFlow.*` differs. | Adopted the simplified-starter idea; GPU fluid and separate output-resolution display are implemented. |
| Bridge | `ftColorBridgeFlow.h` and `ftColorBridgeShader.h` exist only on HD. Bridge headers and shaders differ. | Recorded for future bridge expansion. |
| Shaders | HD adds `ftDensity2PressureShader.h`, `ftInverseShader.h`, `ftMaskShader.h`; many existing shaders differ. | Shader directory scaffold is present; no shader code copied yet. |
| Extensions | HD adds `ftPixelFlow.cpp`; average, mouse, particles, splitvelocity files differ. | Extension scaffolding added; real migration pending. |
| Resolution handling | HD is oriented toward larger input / display workflows. | `FluidSettings::resolutionScale` controls simulation scale and `outputResolutionScale` controls GPU visualization/output scale. |

## Adopted Now

- `example-simple` naming and role under the centralized `examples/` folder.
- Separate input resolution and simulation resolution through `FluidSettings::resolutionScale`.
- Linear-filtered density display for scaled simulation output, so lower simulation scale does not expose large cells in the normal density view.
- Separate GPU visualization/output resolution through `FluidSettings::outputResolutionScale`, with `Fluid2D::outputWidth()` and `outputHeight()` reporting the effective output FBO size.
- Reference comparison found that ofxFlowTools HD also keeps density/output resolution separate from simulation resolution. tcxFlowTools now covers the GPU visualization/output split; exact HD shader-family parity remains a follow-up item.
- Explicit bridge hierarchy retained instead of a single monolithic input processor.

## Not Adopted Yet

- HD shader changes are not copied because TrussC/sokol requires generated cross-backend shader descriptors, explicit bindings, and uniform layout validation.
- HD color/mask bridge passes are not implemented yet.
- HD particle and split-velocity details are not implemented yet.

## Resolution Scale Implementation

`Fluid2D::resize()` stores the requested input size and allocates simulation arrays at:

```text
simWidth  = round(inputWidth  * resolutionScale)
simHeight = round(inputHeight * resolutionScale)
```

The current implementation clamps `resolutionScale` to `0.05..1.0` and uses that size for the GPU solver buffers.

GPU visualization/output FBOs use:

```text
effectiveOutputScale = outputResolutionScale > 0 ? outputResolutionScale : resolutionScale
outputWidth  = round(inputWidth  * effectiveOutputScale)
outputHeight = round(inputHeight * effectiveOutputScale)
```

The current implementation clamps positive `outputResolutionScale` to `0.05..1.0`. Leaving it at `0.0` preserves the older behavior where output follows simulation scale.

`example-hd` now audits this behavior with keys:

- `1`: full-resolution simulation.
- `2`: half-resolution simulation.
- `3`: quarter-resolution simulation.
- `o`: toggle output between full and half resolution.

Automated visual checks can start exact modes with:

```bash
TCX_HD_SCALE=0.25 TCX_HD_OUTPUT_SCALE=1.0 addons/tcxFlowTools/examples/example-hd/bin/example-hd.app/Contents/MacOS/example-hd
```

## Format Fallback Plan

- Desktop native: prefer `RGBA16F` for velocity/density/pressure-style buffers.
- Web: start with `RGBA8` fallback unless renderable float support is confirmed at runtime.
- Pressure/divergence can later use R or RG formats when TrussC exposes reliable support across backends.
