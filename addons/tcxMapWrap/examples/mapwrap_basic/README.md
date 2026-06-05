# mapwrap_basic

Minimal tcxMapWrap example demonstrating the core projection-mapping workflow.

## Features

- Start with a built-in color-bar source mapped onto a quad surface
- Drag the 4 corner handles to warp the projection
- Switch between Presentation / SurfaceEdit modes
- Save and load project files
- Calibration pattern source
- Toggle test pattern overlay
- Polygon mask demo
- Auto-detected language for addon internals; the bitmap overlay uses ASCII labels

## Build & Run (macOS)

```bash
cd examples/mapwrap_basic
trusscli update -p .
trusscli build
trusscli run
```

## Build & Run (Windows)

```bash
cd examples\mapwrap_basic
trusscli update -p .
trusscli build
trusscli run
```

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| 1 | Presentation mode |
| 2 | SurfaceEdit mode |
| S | Save project to `data/mapwrap_basic.json` |
| L | Load project from `data/mapwrap_basic.json` |
| T | Toggle output test pattern |
| M | Add polygon mask to selected surface |
| Esc | Deselect |

## Data

No default media file or project file is required. The startup scene is built in code from built-in pattern sources. `S` and `L` still save/load `data/mapwrap_basic.json` when you want to test persistence.

## iOS Note

On iOS, replace mouse events with touch events using `PointerEvent::touch()`. The `touchesBegan` / `touchesMoved` / `touchesEnded` APIs map directly to `pointerDown` / `pointerMove` / `pointerUp` with `Device::Touch` and the appropriate `pointerId`.
