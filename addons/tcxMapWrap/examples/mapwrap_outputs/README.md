# mapwrap_outputs

Output system demo for tcxMapWrap. Demonstrates output regions, test patterns, color correction, and masks.

## Features

- Default output with full-canvas region
- Output canvas region editing (shrink/expand the mapped area)
- Output test pattern toggle
- Output color correction with presets (Normal / Bright / High Contrast / Desaturated)
- Output mask (vignette demo)
- Output bounds overlay visualization

## Build & Run (macOS)

```bash
cd examples/mapwrap_outputs
trusscli update -p .
trusscli build
trusscli run
```

## Build & Run (Windows)

```bash
cd examples\mapwrap_outputs
trusscli update -p .
trusscli build
trusscli run
```

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| T | Toggle test pattern on output |
| B | Toggle output bounds overlay |
| R | Reset output region to full canvas |
| C | Cycle color correction preset |
| M | Add vignette mask to output |
| + / - | Increase / decrease brightness |
| [ / ] | Shrink / expand canvas region width |
| Esc | Deselect |

## Output Properties

| Property | Type | Description |
|----------|------|-------------|
| `canvasRegionNorm` | Rect (0–1) | Which part of the design canvas maps to this output |
| `displayRegionPixels` | Rect | Physical display position/size in pixels |
| `pixelSize` | Vec2 | Output resolution |
| `rotationDegrees` | float | Output rotation |
| `showTestPattern` | bool | Show calibration pattern on output |
| `colorCorrection` | ColorCorrection | Per-output color adjustments |
| `masks` | vector\<MapWrapMask\> | Output-level masks |

## iOS Note

On iOS, outputs map to external displays via AirPlay or HDMI adapters. The `displayRegionPixels` should match the external display resolution. Use `UIScreen.screens` to detect connected displays.
