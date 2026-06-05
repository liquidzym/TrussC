# mapwrap_touch_sim

Touch input simulation demo for tcxMapWrap. Tests the touch workflow on desktop.

## Features

- Toggle between mouse and touch input modes (F1)
- Switch between mouse/touch hit radius (F2)
- Large touch-friendly handles in touch mode
- No keyboard required for core operations (click to select/drag)
- Two-finger pan/zoom simulation (Alt+drag)
- Visual indicator for current hit radius and input mode

## Build & Run (macOS)

```bash
cd examples/mapwrap_touch_sim
trusscli update -p .
trusscli build
trusscli run
```

## Build & Run (Windows)

```bash
cd examples\mapwrap_touch_sim
trusscli update -p .
trusscli build
trusscli run
```

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| F1 | Toggle mouse / touch mode |
| F2 | Cycle hit radius (small ↔ large) |
| Alt+Drag | Simulate two-finger pan/zoom |
| Esc | Deselect |

## Mouse Interaction

| Action | Mouse Mode | Touch Mode |
|--------|-----------|------------|
| Click | Select / drag handle | Select / drag handle (larger hit area) |
| Alt+Drag | Pan & zoom viewport | — |

## iOS Touch API

On iOS, the equivalent touch events map directly:

- `touchesBegan` → `pointerDown(PointerEvent::touch(pos, pointerId))`
- `touchesMoved` → `pointerMove(PointerEvent::touch(pos, pointerId))`
- `touchesEnded` → `pointerUp(PointerEvent::touch(pos, pointerId))`

Use `pointerId` to distinguish multiple fingers (0, 1, 2…). The engine handles multi-touch gestures internally when it receives events from two or more concurrent pointer IDs.

Default `touchHandleRadiusPixels` is 24px (vs 8px for mouse). Override via `OverlayOptions`.
