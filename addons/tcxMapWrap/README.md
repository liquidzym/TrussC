# tcxMapWrap — TrussC Projection Mapping Addon

**Version:** 0.1.0
**Platforms:** macOS / iOS / Windows only
**Not supported:** Linux / Web / Raspberry Pi / Android

## What Is This?

`tcxMapWrap` is a TrussC-native projection mapping addon. It provides the core mapping engine for the upcoming iPad projection mapping tool, but can also be used on macOS and Windows.

### Key Features

- **Surface types:** Quad, Grid/Bilinear, Bezier, Triangle, Circle, Polygon
- **Warp:** Perspective (homography), Grid bilinear, Catmull-Rom curved, Bezier surface sampling, Perspective+Grid
- **Quad quality:** Quad surfaces have editable `meshResolution` and are tessellated by default to reduce visible two-triangle diagonal artifacts
- **Mask system:** Rectangle, Ellipse, Polygon, Bezier, Freehand, AlphaTexture masks with Add/Subtract/Intersect/Inverted and feather support
- **Output / Stage model:** Multi-output structure, default output auto-created
- **Source management:** Texture, FBO, Video, Image, Generated, Built-in Calibration Patterns
- **Runtime renderer:** TrussC `Mesh::draw(Texture&)` path for Texture/FBO/Image/Video/Generated/Pattern sources
- **Editor:** Mouse / Touch / Pen unified input model, active control-point selection, keyboard cycling, lattice resize, viewport pan/zoom, snap, nudge, align
- **Undo / Redo:** Command stack with localized descriptions
- **Serialization:** JSON v1 full structure with backward compatibility
- **Project validation:** Missing source/media detection, path relinking
- **Geometry validation:** Self-intersection, NaN, too-small, winding detection
- **🎨 i18n:** Auto-detects system language (Chinese ↔ English) with manual override

## Internationalization (i18n)

The addon automatically detects the system language at startup:

- **Chinese** (zh-Hans, zh-Hant, zh_CN, zh_TW, zh-HK…) → defaults to **Chinese** UI
- **Any other language** → defaults to **English** UI
- **Manual override** is always available

### Usage

```cpp
#include "tcxMapWrap/tcxMapWrap.h"
using namespace tcx::mapwrap;

// Auto-detect runs on first tr() call, but you can call it explicitly:
MapWrapI18n::instance().detectAndSetLanguage();

// Manual language switch:
MapWrapI18n::instance().setLanguage("zh");  // Switch to Chinese
MapWrapI18n::instance().setLanguage("en");  // Switch to English

// Reset to auto-detected:
MapWrapI18n::instance().resetToDetected();

// Get translated string:
std::string label = MapWrapI18n::instance().tr("surface.quad");
// Returns "Quad" (English) or "四边形" (Chinese)

// Shorthand:
std::string mode = tr("mode.surface_edit");
// Returns "Surface Editing" or "曲面编辑"

// Language change callback:
MapWrapI18n::instance().onLanguageChange([](const std::string& lang) {
    // Refresh UI with new language
});

// Add custom translations:
MapWrapI18n::TranslationMap myTranslations = {
    {"custom.key", "我的自定义文本"}
};
MapWrapI18n::instance().addTranslations("zh", myTranslations);
```

### Available Language Keys

See `include/tcxMapWrap/MapWrapI18n.h` for the complete list. Categories include:

- `mode.*` — Edit modes
- `surface.*` — Surface types and actions
- `grid.*` / `circle.*` — Surface-specific properties
- `warp.*` — Warp types
- `source.*` — Source types and controls
- `pattern.*` — Calibration patterns
- `mask.*` — Mask types and operations
- `output.*` / `stage.*` — Output and stage
- `blend.*` — Blend modes and settings
- `color.*` — Color correction
- `editor.*` — Editor actions
- `snap.*` / `overlay.*` — Snap and overlay
- `group.*` — Surface groups
- `undo.*` — Undo/redo
- `project.*` — Project management
- `geometry.*` — Geometry validation
- `stats.*` — Render stats
- `common.*` — Common UI strings

## Minimal Example

```cpp
#include <TrussC.h>
#include "tcxMapWrap/tcxMapWrap.h"

using namespace tc;
using namespace tcx::mapwrap;

class MyApp : public App {
public:
    MapWrapEngine mapper;
    Texture image;

    void setup() override {
        // Auto-detect language (runs implicitly, but can be explicit)
        MapWrapI18n::instance().detectAndSetLanguage();

        image.load("test.png");
        auto sourceId = mapper.sources().addTexture("image", &image);

        auto surface = mapper.document().createQuadSurface(tr("surface.quad"));
        surface->setSource(sourceId);
        surface->destinationPoints() = {{
            Vec2(0.1f, 0.1f), Vec2(0.9f, 0.1f),
            Vec2(0.9f, 0.9f), Vec2(0.1f, 0.9f)
        }};
        mapper.document().addSurface(surface);
    }

    void update() override { mapper.update(getDeltaTime()); }
    void draw() override { clear(0.0f); mapper.draw(); }
};
```

## Surface Types

| Type | Description |
|------|-------------|
| Quad | 4-corner perspective warp |
| Grid | Bilinear/Catmull-Rom grid mesh |
| Bezier | Control-lattice Bezier surface sampled into an editable mesh |
| Triangle | 3-point surface |
| Circle | Center + radii + segments |
| Polygon | N-point with ear-clipping triangulation |

Perspective is a Quad property (`perspectiveCorrection`). Grid and Bezier are
separate surface kinds with their own control lattices. Use
`MapWrapEditor::convertSelectedTo()` or the `surfaceKind` editable property to
convert a selected surface while preserving its id, source, masks, blend, color
correction, visibility, opacity, and lock state.

The full editor example exposes practical controls:

- Mouse selects a surface/control point.
- `TAB` cycles the active control point.
- Arrow keys select neighboring points; `Shift` + arrow nudges the active point.
- `X` converts the selected surface through Quad → Grid → Bezier → Quad.
- `O` toggles Quad perspective correction.
- `,` / `.` remove/add columns, `;` / `/` remove/add rows, and `-` / `=` change mesh resolution.

## Mask System

Three levels of masks:
1. **Surface masks** — affect individual surfaces
2. **Output masks** — affect individual outputs
3. **Global masks** — affect the entire composition

Supported mask types: Rectangle, Ellipse, Polygon, Bezier, Freehand, AlphaTexture.
Surface masks are evaluated into per-vertex alpha on the rendered mesh. Feathered
geometry masks and alpha-texture masks are supported; alpha masks can sample
CPU alpha data from Image, Video, Generated, and Built-in Pattern sources when
the addon is compiled inside a TrussC runtime.

## Calibration Patterns

Built-in pattern sources: Checkerboard, Grid, Fine Grid, Crosshair, Corner Labels, UV Gradient, Color Bars, Luma Ramp, Edge Blend Ramp, Alpha Radial, Numbered Cells, Safe Area, Solid Color.

`Alpha Radial` is intended for testing soft/complex alpha masks without external
media files.

## Save / Load

```cpp
// Save
MapWrapSerialization::saveToFile(mapper.document(), "my_project.tcxmap.json");

// Load
auto result = MapWrapSerialization::loadFromFile(mapper.document(), "my_project.tcxmap.json");
if (!result.ok) { logError(result.message); }
for (const auto& w : result.warnings) { logWarning(w); }
```

## Project Validation

```cpp
auto report = mapper.document().validateProject();
if (!report.ok) {
    for (const auto& missing : report.missingSources) {
        logWarning(tr("project.missing_source") + ": " + missing);
    }
}
```

## Platform Notes

### macOS
- Full keyboard shortcuts available in examples
- Retina / high DPI overlay supported
- Metal backend via sokol

### iOS / iPadOS
- Touch input fully supported via PointerEvent
- Touch handle radius: 24px (configurable)
- Editor viewport pinch zoom / pan
- No keyboard dependency for core editing
- Autosave path provided by host app

### Windows
- Visual Studio 2022 compatible
- D3D11 backend via sokol
- UTF-8 source names handled correctly

## Acknowledgements

Inspired by:
- [ofxWarp](https://github.com/prisonerjohn/ofxWarp) — warp abstraction concepts
- [ofxPiMapper](https://github.com/kr15h/ofxPiMapper) — composition and source management concepts
- [ofxBezierSurface](https://github.com/charlesveasey/ofxBezierSurface) and [ofxBezierWarp](https://github.com/gameoverhack/ofxBezierWarp) — Bezier surface reference concepts

This addon is a **TrussC-native implementation** and does not contain code from either project.

## License

MIT License — see [LICENSE.md](LICENSE.md)
