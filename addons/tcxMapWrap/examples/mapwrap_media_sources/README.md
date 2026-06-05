# mapwrap_media_sources

Source system demo showcasing all source types in tcxMapWrap.

## Features

- Image source — static image loaded from file
- Four video source slots — independent TrussC `VideoPlayer` instances when files are present
- FBO source — dynamic scene rendered to a real TrussC FBO
- Generated source — callback timing plus default dynamic renderer texture
- Built-in calibration patterns (13 patterns, cycle with TAB)
- Alpha texture mask using the built-in Alpha Radial source
- Source clock display
- Missing media validation
- Source relinking demo
- Multiple sources on different surfaces simultaneously

## Build & Run (macOS)

```bash
cd examples/mapwrap_media_sources
trusscli update -p .
trusscli build
trusscli run
```

## Build & Run (Windows)

```bash
cd examples\mapwrap_media_sources
trusscli update -p .
trusscli build
trusscli run
```

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| TAB | Cycle calibration pattern |
| R | Relink source (toggle image ↔ calibration) |
| V | Validate project (check missing sources) |
| Space | Play/Pause all video sources |
| 1 / 2 | Presentation / SurfaceEdit mode |
| Esc | Deselect |

## Data

The example is visually self-contained. Missing image/video files render with a placeholder texture, while the FBO, generated texture, calibration pattern, and alpha mask are live by default.

To test real image/video source paths, place media files in a `data/` directory relative to the working directory:

- `data/test_image.png` — image source
- `data/media/opaque_a.mp4` — first opaque video source
- `data/media/alpha_overlay.mov` — alpha-capable video source slot
- `data/media/opaque_b.mp4` — second opaque video source
- `data/media/video_c.mp4` — third video source

If files are missing, the app still opens with visible demo surfaces and validation warnings.

## iOS Note

On iOS, video playback and FBO sources work identically. Use `PointerEvent::touch()` for all interactions. The source clock continues to run in the background; pause it in `onPause()` to conserve resources.
