# tcxMapWrap Architecture

## Overview

```
MapWrapEngine
├── MapWrapDocument          — Persistent composition data
│   ├── MapWrapStage         — Stage / output model
│   │   ├── MapWrapOutput[]  — Individual outputs
│   │   └── globalMasks[]
│   ├── Surface[]            — Mapping surfaces
│   │   └── masks[]          — Per-surface masks
│   ├── SurfaceGroup[]       — Surface grouping
│   └── MapWrapCue[]         — Snapshots
├── SourceRegistry           — Source management
│   ├── SourceTexture
│   ├── SourceFbo
│   ├── SourceVideo
│   ├── SourceImage
│   ├── SourceGenerated
│   └── CalibrationPatternSource
├── MapWrapRenderer          — Rendering pipeline
│   ├── SourceUpdatePass      — Source callbacks/video updates
│   ├── SurfaceMeshPass       — Mesh/UV/index cache
│   ├── MaskPass              — Per-vertex mask alpha coverage
│   ├── SurfaceDrawPass       — TrussC Mesh::draw(Texture&)
│   └── OverlayPass           — Editor/example overlays
├── MapWrapEditor            — Editing control layer
│   ├── EditorViewport       — Pan/zoom/coordinate conversion
│   └── HitTestIndex         — Spatial index
├── UndoStack                — Command stack
└── MapWrapI18n              — Internationalization (singleton)
```

## i18n Architecture

```
MapWrapI18n (singleton)
├── detectSystemLanguage()   — Platform-specific detection
│   ├── iOS/macOS: NSLocale preferredLanguages
│   ├── Windows: GetUserDefaultUILanguage
│   └── Fallback: LANG/LC_ALL env vars
├── zh ↔ en auto-detection
│   ├── "zh*" → "zh" (Chinese)
│   └── everything else → "en" (English)
├── Manual override: setLanguage()
├── Translation lookup: tr(key)
│   ├── Current language → Custom → English fallback → key
└── Language change callbacks
```

The i18n system is thread-safe (mutex-protected) and integrates throughout:
- `Surface::kindName()` → localized surface type names
- `MapWrapMask::kindName()` → localized mask type names
- `Source::kindName()` → localized source type names
- `CalibrationPatternSource::patternName()` → localized pattern names
- `EditableProperty::fromI18n()` → localized inspector labels
- `UndoStack::undoDescription()` → localized undo descriptions

## Dirty Mesh Cache

Surfaces use a `dirty_` flag. Mesh is only rebuilt when geometry changes, not every frame.

Quad surfaces store both `perspectiveCorrection` and `meshResolution`. Even a
four-corner quad is built as a tessellated mesh by default; this avoids the
visible diagonal artifact that appears when a deformed textured quad is drawn as
only two affine triangles. Raising `meshResolution` costs more vertices but
keeps texture curvature smoother.

Bezier surfaces are represented as a persistent 2D control lattice plus a mesh
resolution. Runtime mesh generation samples the Bezier patch into triangles, so
control points and rendered mesh stay in one surface object instead of being
split across independent editor and renderer state.

The editor keeps a persistent active control-point state independent of drag
state. Mouse hit testing, `selectHandle()`, `cycleSelectedHandle()`, and
`selectAdjacentHandle()` all update this state. Grid/Bezier lattice resizing
resamples existing geometry so control points and rendered mesh remain attached
to the same surface object.

Changing from Quad to Grid/Bezier is an explicit surface conversion, not a
hidden property toggle. `MapWrapEditor::convertSelectedTo()` replaces the
surface object with another kind while preserving the same id and common state
such as source, masks, blend, color correction, visibility, opacity, and lock.

When a surface has masks, the renderer requests additional tessellation for
quad surfaces. This prevents soft masks and alpha masks from being sampled only
at four corners. The subdivision is clamped by `PerformanceSettings`.

## Runtime Texture Pipeline

The renderer has two build modes:

- Standalone/test mode prepares CPU-side `SurfaceRenderData` only.
- TrussC runtime mode resolves sources to `Texture` objects and submits a
  TrussC `Mesh` with vertex positions, UVs, color correction brightness, surface
  opacity, blend opacity, and mask alpha.

Runtime source ownership:

- Texture and FBO sources reference externally owned TrussC objects.
- Image sources own a TrussC `Image` and expose CPU pixels for alpha masks.
- Video sources own a TrussC `VideoPlayer`; one source ID means one decoder
  shared by all assigned surfaces.
- Generated sources have callback timing plus a default dynamic texture for
  self-contained examples.
- Built-in patterns generate RGBA pixels on demand, including `AlphaRadial` for
  complex alpha-mask testing.

Hidden video sources can be paused when `PerformanceSettings::pauseVideoWhenHidden`
is enabled. Multiple videos therefore scale by active visible source count, not
by number of surfaces sharing the same source ID.

## Undo Command Model

All edits generate `Command` objects. Pointer drag is merged into a single command (record state at pointer-down, commit at pointer-up).

## Coordinate System

- **Canvas normalized:** 0..1 (all persistent geometry)
- **Source UV:** 0..1 (texture coordinates)
- **Screen pixels:** for hit testing and rendering only
- **EditorViewport:** converts between screen ↔ canvas norm
