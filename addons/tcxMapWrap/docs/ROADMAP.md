# tcxMapWrap Roadmap

## P0 — First Release (0.1.0)

- ✅ Quad / Grid / Bezier / Triangle / Circle / Polygon surfaces
- ✅ SourceRegistry with Image / Video / FBO / Generated / BuiltinPattern
- ✅ MapWrapOutput default output model
- ✅ Surface masks: polygon / ellipse / inverted / add / subtract
- ✅ Surface groups: basic data structure
- ✅ Editor viewport with pan / zoom
- ✅ Pointer / touch unified input
- ✅ Active control-point selection, keyboard cycling, and lattice resize controls
- ✅ Overlay options
- ✅ Snap / nudge / copy-paste geometry / copy-paste UV API
- ✅ JSON v1 full structure
- ✅ Undo / redo with localized descriptions
- ✅ Project validation
- ✅ Geometry validation
- ✅ **i18n: Auto-detect Chinese/English with manual override**
- ✅ Renderer pass structure
- ✅ Textured Quad tessellation to reduce diagonal two-triangle artifacts
- ✅ Opacity / brightness
- ✅ Normal / Add blend mode
- ✅ macOS / iOS / Windows compilation

## P1 — Enhanced Features

- SurfacePolygon complete implementation
- Freehand / Alpha texture mask polish
- Cue / snapshot simple apply
- Autosave
- Collect media packaging
- Color correction shader: contrast / saturation / lift / gain / black/white level
- Group transform: rotation / scale
- Numeric inspector complete coverage
- HitTestIndex bounding box cache
- More complete output correction example

## P2 — Future Enhancements

- Real multi-physical-output window management
- Multi-projector automatic calibration
- Advanced edge blend shader
- 1D / 3D LUT
- Camera calibration data import
- OSC / MIDI
- Remote control
- Quadtree hit-test index
- Background async mesh building
- Cue transition editor
- Timeline
- NDI / Spout / Syphon
- LED processor protocol
- Additional language support (Japanese, Korean, etc.)
