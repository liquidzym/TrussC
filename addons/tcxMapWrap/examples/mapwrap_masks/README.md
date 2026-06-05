# mapwrap_masks

Mask system demo for tcxMapWrap. Shows all mask types and operations.

## Features

- Polygon mask (diamond shape on surface 1)
- Ellipse mask (center circle on surface 2)
- Inverted mask (inverted ellipse on surface 3)
- Subtract mask (cutout from full area on surface 4)
- Interactive mask adding (surface 5)
- Bezier mask on a Bezier surface (surface 6)
- Mask point dragging via editor
- Mask save/load (masks are included in project serialization)
- Touch-simulated mask editing (F1 to toggle)
- Mask validation warnings displayed in overlay

## Build & Run (macOS)

```bash
cd examples/mapwrap_masks
trusscli update -p .
trusscli build
trusscli run
```

## Build & Run (Windows)

```bash
cd examples\mapwrap_masks
trusscli update -p .
trusscli build
trusscli run
```

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| 1-6 | Select demo surface |
| M | Add polygon mask to selected surface |
| B | Add Bezier mask to selected surface |
| E | Add ellipse mask to selected surface |
| I | Toggle invert on last mask |
| X | Cycle mask operation (Add → Subtract → Intersect) |
| S | Save project (includes masks) |
| L | Load project |
| F1 | Toggle touch mode |
| Esc | Deselect |

## Mask Types

| Kind | Description | Data |
|------|-------------|------|
| Polygon | Freeform shape | `points` vector |
| Ellipse | Oval/ellipse | `rect` (bounding box) |
| Rectangle | Axis-aligned rect | `rect` |
| Bezier | Bézier curves | `points` (control points) |
| Freehand | Drawn freehand | `points` vector |
| AlphaTexture | Texture-based | `alphaTextureSource` |

## Mask Operations

| Operation | Effect |
|-----------|--------|
| Add | Include this region (union) |
| Subtract | Exclude this region (difference) |
| Intersect | Only show intersection |

## iOS Note

On iOS, mask points are dragged with touch events. Use `PointerEvent::touch()` with the appropriate `pointerId`. The touch hit radius for mask points defaults to 24px, configurable via `OverlayOptions::touchHandleRadiusPixels`.
