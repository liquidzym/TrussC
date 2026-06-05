# tcxMapWrap Audit Report - 2026-06-05

## Scope

- Path: `/Users/mac/Desktop/TrussC/addons/tcxMapWrap`
- Branch observed: `showlab`
- Goal: audit implementation, fix correctness/build issues, improve maintainability, and verify tests/examples.

## Fixed Issues

### Build And Test Infrastructure

- Added a top-level CMake `project()` declaration so the addon can configure as a standalone test project.
- Added `TCX_MAPWRAP_BUILD_TESTS` and a `tcxMapWrap_tests` CTest target.
- Fixed test compile issues around value-returning APIs and exposed undo command classes needed by `tests/test_undo.cpp`.

### Example Programs

- Updated all six examples for current TrussC API usage:
  - Replaced `getLastFrameTime()` with `getDeltaTime()`.
  - Updated editor key handling to `KeyEventArgs`.
  - Replaced `drawRectangle()` with `drawRect()`.
  - Qualified `tcx::mapwrap::Rect` where TrussC `Rect` was ambiguous.
  - Fixed `SourceRegistry::get()` usage with `std::shared_ptr`.
  - Fixed touch simulation grid initialization by setting grid points explicitly.
- Corrected example fallback `TRUSSC_DIR` from `../../../core` to `../../../../core`, so direct `cmake -S` works without generated presets.
- Added `examples/shared/MapWrapDemoDraw.h`, a self-contained example drawing layer that turns prepared surface meshes into visible TrussC colored meshes with wireframes, handles, masks, labels, and output bounds.
- Changed default visible sources in `basic`, `editor`, `masks`, `outputs`, and `touch_sim` from missing `data/test_image.png` paths to built-in pattern sources.
- Kept `media_sources` conceptually demonstrating image/video/FBO/generated/pattern sources, but it now draws color-coded surfaces even when real media files are absent.
- Replaced localized mode text in bitmap overlays with ASCII mode names, because `drawBitmapString()` does not render CJK glyphs and showed square boxes for Chinese mode names.
- Added bitmap-text sanitizing helpers for all example overlays. Non-ASCII surface/source/output/undo labels now fall back to readable ASCII text instead of rendering square boxes.
- Synced `MapWrapEngine::setCanvasSize()` into the editor viewport, fixing mouse hit testing in examples where drawing used the window size but editor input still used the default 1920x1080 viewport.
- Limited `mapwrap_basic` and `mapwrap_media_sources` to Presentation and Surface Edit modes, because TextureEdit/SourceAssign did not have a visible UI in those examples and appeared broken to users.
- Reworked `mapwrap_editor` to launch with a visible multi-surface scene: Quad, Bilinear/Grid, Bezier, Triangle, Circle, and Polygon.
- Added Bezier mask coverage to `mapwrap_masks` and marked mask edits dirty so render caches and saved state stay synchronized.

### Surface Type Coverage

- Added `SurfaceBezier` as a first-class wrap surface, not just a mask type.
- Implemented Bezier control-lattice editing, hit testing, validation, mesh generation, undo snapshots, copy/paste geometry, property editing, and JSON round-trip.
- Kept Grid/Bilinear as a separate type. Bilinear interpolation is not equivalent to Bezier; it remains the grid-warp path while Bezier is a sampled Bernstein patch.
- Updated example overlay drawing so Bezier control lattices and handles are visible and draggable in the same coordinate system as the rendered mesh.
- Added explicit selected-surface conversion through `MapWrapEditor::convertSelectedTo()` and the `surfaceKind` editable property. Conversion keeps the same surface ID plus source, masks, blend, color correction, visibility, opacity, and lock state.
- Added Quad `meshResolution` as a persisted property. Quad meshes now tessellate by default, reducing the visible two-triangle diagonal artifact under perspective/bilinear-style deformation.

### Interaction And Editing Controls

- Added persistent active-control-point state to `MapWrapEditor`, independent of temporary drag state.
- Added public active-handle APIs: `selectedHandleKind()`, `selectedHandleIndex()`, `selectHandle()`, `cycleSelectedHandle()`, and `selectAdjacentHandle()`.
- Changed keyboard handling so `TAB` cycles the active point and arrow keys select neighboring points with wrap-around. `nudgeSelectedHandle()` now moves the active point, not only a handle currently being dragged.
- Added Grid/Bezier lattice controls: add/remove columns and rows, with geometry resampled instead of separating mesh and control points.
- Added `adjustSelectedMeshResolution()` for Quad/Grid/Bezier.
- Updated example overlays to draw the active control point with a distinct pulsing highlight.
- Updated `mapwrap_editor` shortcuts: arrows select points, `Shift+arrows` nudges the active point, `X` converts Quad/Grid/Bezier, `O` toggles Quad perspective, punctuation shortcuts resize the lattice and mesh resolution.

### Taskbook Parity And Remaining Gaps

- P0 geometry/source/document/editor/test structure is broadly covered: Quad, Grid, Triangle, Circle, Polygon, Bezier surface extension, SourceRegistry, output/stage data model, masks, calibration patterns, JSON, undo, validation, examples, and tests.
- Implemented the runtime textured renderer path: in TrussC builds, `MapWrapRenderer` resolves Texture/FBO/Image/Video/Generated/Pattern sources and submits textured meshes through TrussC `Mesh::draw(Texture&)`.
- Implemented renderer-side surface mask coverage: Add/Subtract/Intersect/Inverted masks are evaluated into per-vertex alpha, with automatic masked quad subdivision, feather support, and AlphaTexture sampling.
- Implemented `SourceVideo` as the owner/manager of a TrussC `VideoPlayer`. Multiple surfaces sharing one source ID share one decoder/player, while separate video sources create separate players.
- Added `AlphaRadial` built-in pattern source for self-contained soft/complex alpha mask examples.
- Wired performance settings into renderer/source behavior for masked subdivision clamping and hidden-video pause/resume.
- Remaining scalability gap: hit testing is correct for the current examples, but P1 bounding-box cache / P2 quadtree acceleration is still open for large surface counts.
- Remaining renderer gap: output/global masks are still represented in the data model but the final output-level compositing pass is not yet a separate offscreen pass. Surface-level masks now render correctly.

### Performance Review

- Multiple mapping regions affect per-frame cost linearly by visible surface count and draw call count. Mesh data is rebuilt only when surface revision/cache changes, so static surfaces do not rebuild every frame.
- Quad/Triangle/Circle surfaces are cheap. Grid surfaces scale as `(cols * meshResolution + 1) * (rows * meshResolution + 1)` vertices and `cols * rows * meshResolution^2 * 6` indices. Bezier surfaces scale as `(resolution + 1)^2` vertices and `resolution^2 * 6` indices.
- Added mesh vector reservation in Quad, Grid, Bezier, Triangle, Circle, and Polygon builders to reduce allocation churn during edits and resize-driven rebuilds.
- Changed renderer stale-cache cleanup from nested ID scans to hash-set lookup, reducing cleanup from O(n^2) to O(n) for many surfaces.
- Multiple video sources are now real `SourceVideo`/`VideoPlayer` instances. Cost scales by active source count: 3-4 simultaneous videos means 3-4 decoders and texture uploads, unless several surfaces share the same `SourceId`.
- Hidden video sources are paused when `PerformanceSettings::pauseVideoWhenHidden` is enabled. This reduces decode/upload load for hidden surfaces but does not reduce cost for multiple visible videos.
- Quad surfaces are tessellated by their own `meshResolution` even without masks, so deformed textured quads do not fall back to a single two-triangle draw. When masks are enabled, the renderer can request additional subdivision clamped by `PerformanceSettings::maxGridSubdivision`.

### Recommended Next Improvements

- Add a dedicated output/global mask compositing pass for final projector output regions, separate from per-surface alpha.
- Add optional decode throttling or source budget warnings for many simultaneous visible videos.
- Add richer blend/color-correction shader paths for multiply/screen/add and per-output correction; current runtime draw path uses alpha blending plus vertex color brightness/opacity.
- Wire `reduceOverlayDetailWhileDragging` into example/editor overlays for very dense scenes.
- Add a stress/benchmark example for 10, 50, and 100 mapping regions plus several active video-like sources, and report frame time, mesh rebuild count, draw calls, and source update count.
- Add hit-test acceleration for large scenes through a bounding-box cache first, with quadtree/spatial index only if the benchmark shows the need.

### Document And Surface Ownership

- Changed `MapWrapDocument::create*Surface()` methods to return typed surface pointers.
- Made `create*Surface()` construct only; callers now explicitly add surfaces through `addSurface()` or `insertSurface()`.
- Added duplicate-ID guards in `addSurface()` and `insertSurface()` to prevent accidental double insertion.
- Updated `MapWrapEditor` creation paths to push undoable create commands after the create-only behavior change.

### Undo Stack

- Moved concrete undo command declarations into `UndoStack.h` for tests and public editor integrations.
- Made `UndoStack::push()` execute the command before recording it.
- Added duplicate guards for add/delete surface and mask commands.
- Fixed grid control-point indexing: handle stride now uses `cols() + 1`, with row/column bounds checked correctly.

### Source Registry

- Aligned the header with implementation storage by using `<map>`.
- Added non-const `SourceRegistry::get()` for mutable shared source access.
- Added `SourceRegistry::count()` for example and UI source counts.
- Added fixed-ID `SourceRegistry::add()` and `clear()` for source-aware project loading.
- Implemented `SourceGenerated::update()`, so generated sources now run their callbacks once per `MapWrapEngine::update()` frame.
- Added renderer-side default dynamic texture generation for `SourceGenerated`, so examples are visually self-contained even when the callback only records timing/intent.
- Added real runtime storage for source handles: `SourceTexture` stores the external texture pointer, `SourceFbo` stores the external FBO pointer, `SourceImage` owns a TrussC `Image`, and `SourceVideo` owns a TrussC `VideoPlayer`.

### Serialization And Loading

- Added source-aware `MapWrapSerialization` overloads that save/load `SourceRegistry` alongside `MapWrapDocument`.
- Changed examples that save/load projects to use source-aware serialization and create their `data/` directory before saving.
- Changed project loading to replace existing document state instead of appending loaded surfaces onto the current scene.
- Fixed grid serialization to save/load all `(cols + 1) * (rows + 1)` control points, including boundary points.

### Homography And Grid Correctness

- Replaced the old 9x9 Jacobi/SVD homography path with a compact 8x8 four-point linear solve.
- Added scale-aware near-collinear quad rejection.
- Removed unused SVD/Jacobi helper code.
- Corrected inverse homography test semantics by normalizing projective scale before identity comparison.
- Reworked `SurfaceGrid::setCols()`, `setRows()`, `addColumn()`, and `addRow()` to resample by interpolation and preserve boundaries.

### Project Validation

- Added document-level detection of path-like source references as missing sources.
- Added warnings for non-`src_` source IDs that require registry validation.

### Metadata

- Added addon metadata fields: `author`, `trussc_version`, `screenshot`, and `category`.

## Verification

Commands run successfully:

```sh
cmake --build build-tests --target tcxMapWrap_tests
ctest --test-dir build-tests --output-on-failure
./build-tests/tcxMapWrap_tests
jq empty addon.json
cmake -S examples/mapwrap_basic -B /tmp/tcxMapWrap_fallback_basic
cmake -S examples/mapwrap_editor -B /tmp/tcxMapWrap_fallback_editor
cmake -S examples/mapwrap_masks -B /tmp/tcxMapWrap_fallback_masks
cmake -S examples/mapwrap_media_sources -B /tmp/tcxMapWrap_fallback_media_sources
cmake -S examples/mapwrap_outputs -B /tmp/tcxMapWrap_fallback_outputs
cmake -S examples/mapwrap_touch_sim -B /tmp/tcxMapWrap_fallback_touch_sim
trusscli build  # run in all six example directories
```

CTest result after the final renderer/video/mask pass:

- `100% tests passed, 0 tests failed out of 1`
- Internal suite: `119 passed, 0 failed, 119 total`

New regression coverage added:

- Renderer rebuilds when a surface revision changes.
- Undo/control-point movement marks surfaces dirty and keeps mesh output synchronized.
- Generated sources update exactly once per engine frame.
- Engine canvas size drives editor hit testing, so mouse dragging a visible handle changes the rendered mesh.
- Source registry round-trips through project serialization.
- Loading replaces the existing document instead of appending.
- Grid control-point serialization preserves boundary points.
- Bezier surface mesh counts, control-point hit testing, corner evaluation, and serialization round-trip.
- Quad default mesh resolution produces a subdivided mesh and serializes through JSON.
- Editor active-handle adjacent selection wraps over grid points.
- Editor Grid/Bezier lattice row/column controls resize the selected surface.
- Quad to Bezier conversion preserves common state and active selection.
- Feathered ellipse masks produce partial renderer alpha.
- Complex Add/Subtract masks reduce renderer alpha correctly.
- Masked quads subdivide for alpha/feather mask coverage.
- Alpha Radial built-in pattern contains soft alpha coverage.

Each example configured with `cmake --preset macos` and built with `trusscli build`:

- `examples/mapwrap_basic`
- `examples/mapwrap_editor`
- `examples/mapwrap_masks`
- `examples/mapwrap_media_sources`
- `examples/mapwrap_outputs`
- `examples/mapwrap_touch_sim`

All six produced macOS `.app` bundles under their respective `bin/` directories.

After the renderer/video/mask pass, all six examples were rebuilt successfully again. The final rebuilds emitted no compile or link failures.

After the interaction/quad-subdivision pass, all six examples were rebuilt successfully again:

- `examples/mapwrap_basic`
- `examples/mapwrap_editor`
- `examples/mapwrap_masks`
- `examples/mapwrap_media_sources`
- `examples/mapwrap_outputs`
- `examples/mapwrap_touch_sim`

`mapwrap_editor` was smoke-launched after rebuild, remained running without startup errors for several seconds, and was then terminated.

GUI smoke verification:

- Launched `mapwrap_editor` and captured `/tmp/tcxMapWrap_screens_bezier/mapwrap_editor_compact.png`.
- Launched `mapwrap_masks` and captured `/tmp/tcxMapWrap_screens_bezier/mapwrap_masks.png`.
- Screenshots confirm visible default Quad, Grid/Bilinear, Bezier, Triangle, Circle, and Polygon surfaces, plus visible Bezier mask controls.
- Launched `mapwrap_masks` after the alpha-mask pass and captured `/tmp/tcxMapWrap_masks_color_alpha.png`; screenshot confirms a visible alpha-texture mask using the built-in Alpha Radial source.
- Launched `mapwrap_media_sources` after the runtime renderer pass and captured `/tmp/tcxMapWrap_media_sources_generated.png`; screenshot confirms real FBO rendering, generated dynamic texture rendering, multiple video slots with placeholder fallback, and alpha-masked video slot behavior.

## Notes

- GUI apps were launched directly for smoke screenshots and then terminated automatically. Full manual UX validation is still useful for deeper workflows, but the previous blank/default-text issues and viewport hit-test regression now have automated or screenshot evidence.
- Generated build outputs, `.app` bundles, generated presets, and local taskbook files remain ignored by `addons/tcxMapWrap/.gitignore`.
