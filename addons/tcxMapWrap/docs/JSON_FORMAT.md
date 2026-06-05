# tcxMapWrap JSON Format

## Schema

```json
{
  "schema": "tcxMapWrap.composition",
  "version": 1,
  "name": "default",
  "designCanvasSize": [1920, 1080],

  "createdWith": {
    "addon": "tcxMapWrap",
    "addonVersion": "0.1.0"
  },

  "language": "auto",

  "stage": {
    "outputs": [],
    "globalMasks": [],
    "calibration": {
      "defaultPattern": "grid",
      "showPatternOnLoad": false
    }
  },

  "sources": [],
  "surfaceGroups": [],
  "surfaces": [],
  "presets": [],
  "cues": [],

  "workspace": {
    "editMode": "surfaceEdit",
    "selectedSurface": "",
    "editorViewport": {
      "panPixels": [0, 0],
      "zoom": 1.0
    }
  }
}
```

## Language Field

The `"language"` field at top level controls the saved language preference:

- `"auto"` — Use system auto-detection (default)
- `"zh"` — Force Chinese
- `"en"` — Force English

When loading, if `"language"` is `"auto"` or missing, the addon calls `MapWrapI18n::instance().detectAndSetLanguage()`. If a specific language code is set, it calls `setLanguage()` directly.

## Backward Compatibility

| Missing Field | Behavior |
|---------------|----------|
| `stage` | Auto-create default stage with one output |
| `surfaceGroups` | Treated as empty array |
| `masks` (on surfaces) | Treated as empty array |
| `workspace` | Ignored (editor state only) |
| `language` | Auto-detect from system |
| Unknown fields | Silently ignored |

## Surface Fields

Common surface fields include `id`, `kind`, `name`, `visible`, `locked`,
`source`, `opacity`, `sourceRect`, `blend`, `colorCorrection`, and `masks`.

Quad-specific fields:

```json
{
  "kind": "quad",
  "destinationPoints": [[0.1, 0.1], [0.9, 0.1], [0.9, 0.9], [0.1, 0.9]],
  "uvPoints": [[0, 0], [1, 0], [1, 1], [0, 1]],
  "perspectiveCorrection": true,
  "meshResolution": 16
}
```

`meshResolution` is optional for older projects. If omitted, the runtime uses
the current Quad default. Higher values reduce visible two-triangle deformation
at the cost of more vertices.

Grid and Bezier surfaces also persist their control dimensions and
`meshResolution`. Changing a surface between Quad/Grid/Bezier at runtime keeps
the surface id and common fields, but the saved `kind` becomes the actual
converted type.
