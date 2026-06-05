# tcxMapWrap API Reference

## Core Classes

### MapWrapEngine
Top-level entry point. Aggregates document, renderer, editor, sources, and undo stack.

| Method | Description |
|--------|-------------|
| `document()` | Access the composition document |
| `renderer()` | Access the renderer |
| `editor()` | Access the editor |
| `sources()` | Access the source registry |
| `undoStack()` | Access the undo/redo stack |
| `update(dt)` | Update all subsystems |
| `draw()` | Render the current composition |
| `setCanvasSize(pixels)` | Set the output canvas size |
| `performanceSettings()` | Get/set performance config |
| `stats()` | Get render statistics |

### MapWrapDocument
Persistent data model for the composition.

| Method | Description |
|--------|-------------|
| `stage()` | Access the stage/output model |
| `createQuadSurface(name)` | Create a new quad surface |
| `createGridSurface(cols, rows)` | Create a new grid surface |
| `createBezierSurface(controlCols, controlRows)` | Create a new Bezier surface |
| `addSurface(surface)` | Add surface to document |
| `removeSurface(id)` | Remove surface by ID |
| `surfaces()` | Get all surfaces |
| `groups()` | Get all surface groups |
| `createCueFromCurrentState(name)` | Snapshot current state as cue |
| `validateProject()` | Check for missing sources/media |
| `isDirty()` | Check if document has unsaved changes |

### MapWrapEditor
Editing control layer (not a complete UI).

| Method | Description |
|--------|-------------|
| `setMode(mode)` | Switch edit mode |
| `pointerDown/Move/Up(e)` | Unified pointer/touch input |
| `selectSurface(id)` | Select a surface |
| `selectedHandleKind()` / `selectedHandleIndex()` | Inspect the active control point |
| `selectHandle(kind, index)` | Select a specific control point |
| `cycleSelectedHandle(delta)` | Cycle active control point, wrapping at the end |
| `selectAdjacentHandle(dx, dy)` | Move active point selection across a grid/lattice or around vertices |
| `deleteSelected()` | Delete selected surface |
| `convertSelectedTo(kind)` | Convert selected surface type while preserving id/source/masks/common state |
| `addColumnToSelected()` / `removeColumnFromSelected()` | Resize selected Grid/Bezier horizontally |
| `addRowToSelected()` / `removeRowFromSelected()` | Resize selected Grid/Bezier vertically |
| `adjustSelectedMeshResolution(delta)` | Change Quad/Grid/Bezier tessellation density |
| `nudgeSelected(delta)` | Move selected surface |
| `nudgeSelectedHandle(delta)` | Move the active control point |
| `fitSelectedToCanvas()` | Fit selected to canvas |
| `copySelectedGeometry()` | Copy geometry to clipboard |
| `selectedProperties()` | Get editable properties |
| `snapSettings()` | Get/set snap configuration |

### MapWrapI18n
Internationalization singleton with auto-detection.

| Method | Description |
|--------|-------------|
| `instance()` | Get singleton |
| `detectAndSetLanguage()` | Auto-detect system language |
| `setLanguage(code)` | Manual language switch |
| `language()` | Get current language code |
| `isChinese()` | Check if current language is Chinese |
| `tr(key)` | Get translated string |
| `addTranslations(lang, map)` | Add custom translations |
| `onLanguageChange(cb)` | Register language change callback |
| `resetToDetected()` | Reset to auto-detected language |

## Surface Hierarchy

```
Surface (abstract base)
├── SurfaceQuad
├── SurfaceGrid
├── SurfaceBezier
├── SurfaceTriangle
├── SurfaceCircle
└── SurfacePolygon
```

All surfaces support: visibility, lock, opacity, source assignment, source rect, blend settings, color correction, masks.

Warp type model:

- `SurfaceQuad` is the four-corner surface. `perspectiveCorrection` switches perspective/homography-style mapping, and `meshResolution` controls quad tessellation. The default mesh resolution is greater than 1 so textured quads do not show a visible two-triangle diagonal under deformation.
- `SurfaceGrid` is the bilinear/curved control-grid surface. Columns/rows are editable at runtime.
- `SurfaceBezier` is a real Bezier control lattice sampled into a dense mesh. Control rows/columns are editable at runtime.
- `convertSelectedTo(SurfaceKind::Grid/Bezier/Quad/...)` changes the selected surface kind explicitly. It keeps the same surface id and common state so source assignments, masks, blend/color settings, and external references continue to work.

## Source Hierarchy

```
Source (abstract base)
├── SourceTexture
├── SourceFbo
├── SourceVideo
├── SourceImage
├── SourceGenerated
└── CalibrationPatternSource (BuiltinPattern)
```

Runtime notes:

- `SourceTexture` and `SourceFbo` keep external TrussC texture/FBO handles by reference.
- `SourceImage` owns a TrussC `Image` runtime when compiled in a TrussC app.
- `SourceVideo` owns a TrussC `VideoPlayer`; several surfaces assigned to the same source ID share one decoder/player.
- `SourceGenerated` stores callback intent and also has a default renderer-side dynamic texture so examples remain visually self-contained.
- Built-in pattern sources generate RGBA pixels and GPU textures on demand.

## Renderer

`MapWrapRenderer` always prepares `SurfaceRenderData` for tests, overlays, and host inspection. In a TrussC runtime build it also submits textured meshes through `trussc::Mesh::draw(const Texture&)`.

Important options and stats:

| API | Description |
|-----|-------------|
| `RenderOptions::submitTexturedMeshes` | Enable/disable GPU textured submission |
| `supportsGpuDrawing()` | True when compiled with TrussC runtime support |
| `lastDrawSubmittedGpu()` | Whether the previous draw submitted at least one GPU textured mesh |
| `RenderStats::texturedDrawCount` | Surfaces drawn with real resolved textures |
| `RenderStats::placeholderDrawCount` | Surfaces drawn with fallback placeholder texture |
| `RenderStats::activeVideoSourceCount` | Visible, intended-playing video sources |
| `RenderStats::pausedVideoSourceCount` | Hidden, intended-playing video sources paused by performance settings |
| `RenderStats::alphaMaskCount` | AlphaTexture masks evaluated this frame |
| `RenderStats::featheredMaskCount` | Feathered masks evaluated this frame |

## Masks

Surface masks are evaluated into a per-vertex alpha array on the tessellated mesh. Add masks use max coverage, Subtract masks multiply by `(1 - coverage)`, and Intersect masks multiply by coverage. Quad meshes are tessellated by `SurfaceQuad::meshResolution()` even without masks to reduce affine two-triangle artifacts; when masks are present, the renderer can request additional subdivision within `PerformanceSettings::maxGridSubdivision` so feathered and alpha masks have enough vertices to show smooth coverage.

## Enumerations

- `SurfaceKind`: Quad, Grid, Bezier, Triangle, Circle, Polygon
- `WarpKind`: None, Perspective, Grid, PerspectiveGrid
- `SourceKind`: None, Texture, Fbo, Video, Image, Generated, BuiltinPattern
- `EditMode`: Presentation, SurfaceEdit, TextureEdit, SourceAssign, MaskEdit, OutputEdit
- `MaskKind`: Rectangle, Ellipse, Polygon, Bezier, Freehand, AlphaTexture
- `BlendMode`: Normal, Add, Multiply, Screen, Lighten, Darken, AlphaMask
