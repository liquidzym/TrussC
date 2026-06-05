# mapwrap_editor

Full-featured editor example demonstrating the complete MapWrapEditor API.

## Features

- Add quad, grid/bilinear, Bezier, triangle, circle, and polygon surfaces
- Visible default scene with all major surface types on first launch
- Delete and duplicate surfaces
- Layer ordering (bring forward / send backward)
- Lock and visibility toggles per surface
- Undo/Redo support
- Polygon mask editing
- Snap-to-grid toggle
- Copy/paste geometry and UV coordinates
- Active control-point selection with mouse, TAB, and arrow keys
- Grid/Bezier lattice row/column editing
- Quad/Grid/Bezier conversion on the selected surface
- Quad perspective toggle and mesh-resolution adjustment
- Geometry validation warnings overlay
- Debug overlay with FPS, selection, mode, stats
- Language toggle (Chinese / English)

## Build & Run (macOS)

```bash
cd examples/mapwrap_editor
trusscli update -p .
trusscli build
trusscli run
```

## Build & Run (Windows)

```bash
cd examples\mapwrap_editor
trusscli update -p .
trusscli build
trusscli run
```

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Q | Add quad surface |
| G | Add grid surface |
| B | Add Bezier surface |
| T | Add triangle surface |
| C | Add circle surface |
| P | Add polygon surface |
| 1-6 | Switch edit mode |
| Delete | Delete selected surface |
| D | Duplicate selected surface |
| [ / ] | Send backward / Bring forward |
| L | Toggle lock on selected |
| V | Toggle visibility on selected |
| Z | Undo |
| Shift+Z | Redo |
| M | Add polygon mask to selected |
| S | Toggle snap |
| U | Copy UV of selected |
| Shift+U | Paste UV to selected |
| Ctrl+C | Copy geometry |
| Ctrl+V | Paste geometry |
| Tab | Select next control point |
| Arrow keys | Select neighboring control point |
| Shift+Arrow keys | Nudge active control point |
| X | Convert selected surface Quad → Grid → Bezier → Quad |
| O | Toggle Quad perspective correction |
| , / . | Remove / add control columns |
| ; / / | Remove / add control rows |
| - / = | Decrease / increase mesh resolution |
| F9 | Toggle language (zh ↔ en) |
| Esc | Deselect |

## Mouse

- Click to select a surface or make a control point active
- Drag handles to warp
- Drag body to move

## iOS Note

On iOS, use `PointerEvent::touch()` for all pointer interactions. Touch handles are larger (24px radius) by default in `OverlayOptions`. No keyboard shortcuts are available; provide a touch-friendly toolbar instead.
