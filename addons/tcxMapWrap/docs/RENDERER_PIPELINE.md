# Renderer Pipeline

## Pass Structure

```
1. SourceUpdatePass
   └─ Update video frames, generated sources, clock

2. SurfaceMeshPass
   └─ Rebuild dirty meshes (quad, grid, triangle, circle, polygon)

3. SurfaceDrawPass
   └─ Draw each visible surface with source texture
   └─ Apply opacity, brightness, blend mode

4. MaskPass
   └─ Apply surface masks, output masks, global masks

5. OutputCorrectionPass
   └─ Apply output color correction, edge blend

6. OverlayPass
   └─ Draw editor handles, outlines, names, mask points
   └─ Only in non-Presentation mode

7. DebugPass (optional)
   └─ FPS, stats, geometry warnings
```

## Shaders

| Shader | Purpose | P0 |
|--------|---------|----|
| `mapwrap_textured.slang` | Textured mesh rendering | ✅ |
| `mapwrap_mask.slang` | Mask alpha rendering | ✅ |
| `mapwrap_edge_blend.slang` | Edge blend | P1 |
| `mapwrap_color_correction.slang` | Color correction | P1 |

## Backend

- **macOS / iOS:** Metal via sokol_gfx
- **Windows:** D3D11 via sokol_gfx
- **No OpenGL direct calls**
- Shaders cross-compiled via sokol-shdc

## Render Stats

```cpp
struct RenderStats {
    int drawnSurfaceCount;
    int skippedSurfaceCount;
    int rebuiltMeshCount;
    int missingSourceCount;
    int invalidSurfaceCount;
    int maskCount;
};
```

Stats are available via `MapWrapEngine::stats()` and should be displayed in debug overlay.

## Quad Tessellation

Textured quad rendering uses mesh tessellation rather than a single two-triangle
quad by default. `SurfaceQuad::meshResolution()` controls the base subdivision
and defaults to 16. This is intentional: with only two triangles, UVs are
interpolated affinely per triangle and a deformed quad shows a diagonal seam or
triangular distortion. Subdivision approximates perspective/bilinear warps
smoothly and also gives masks enough vertices for feathered alpha coverage.

The renderer may request additional subdivision through
`MeshBuildContext::meshSubdivision` when masks are active. The effective quad
subdivision is the maximum of the surface's own `meshResolution` and the
renderer-requested mask subdivision, clamped by the surface setter and
`PerformanceSettings::maxGridSubdivision`.
