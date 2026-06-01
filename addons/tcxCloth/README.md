# tcxCloth

tcxCloth is a GPU-oriented TrussC rewrite of the cloth simulation idea used by
ofxCloth. It does not depend on openFrameworks, glm, or OpenGL immediate mode.

This addon is a clean TrussC implementation, not a source copy of either
referenced ofxCloth repository. The GitHub license endpoint returned no license
metadata for `kashimAstro/ofxCloth` or `mimetikxs/ofxCloth` when this addon was
created, so no upstream source files are included.

## Backend Modes

- `Auto`: prefers the GPU TexturePingPong backend when TrussC has a valid
  graphics context and float render targets are available; otherwise it falls
  back to `CpuReference`.
- `TexturePingPong`: cross-platform shader/FBO position solver used by the
  examples on supported GPU backends.
- `ComputeStorageBuffer`: reserved for future TrussC/sokol compute support.
- `CpuReference`: CPU Verlet/Jacobi solver used for tests, debug, unsupported
  platforms, and fallback.

The current implementation keeps the GPU-oriented API, topology, and static
triangle and wire index buffers. TexturePingPong drives the visible examples
with GPU position ping-pong; CPU readback is still used to update the TrussC
mesh until a dedicated GPU render path lands.

See `GPU_NOTES.md` for implementation notes, GPU validation findings, and
follow-up constraints.

## Supported Platforms

The CPU reference backend is standard C++20 and uses only TrussC types, so it is
intended to compile on macOS, Windows, Web, and Linux wherever TrussC builds.
TexturePingPong is implemented through TrussC's sokol shader flow and is
currently validated on macOS Metal; Windows D3D11 and Web are intended targets
that still need visual validation.

## Add To A TrussC App

Add this to `addons.make`:

```text
tcxCloth
```

Then include the umbrella header:

```cpp
#include <tcxCloth.h>
```

Basic setup:

```cpp
tcxCloth::Cloth cloth;

void setup() {
    tcxCloth::ClothSettings settings;
    settings.columns = 64;
    settings.rows = 64;
    settings.width = 520.0f;
    settings.height = 360.0f;
    settings.origin = tc::Vec3(120.0f, 80.0f, 0.0f);
    settings.backend = tcxCloth::ClothSettings::SolverBackend::Auto;

    cloth.setup(settings);
    cloth.pinTopEdge();
    cloth.setGravity(tc::Vec3(0.0f, 420.0f, 0.0f));
    cloth.setWind(tc::Vec3(0.0f, 0.0f, 1.0f), 2.0f);
}

void update() {
    cloth.update(static_cast<float>(tc::getDeltaTime()));
}

void draw() {
    cloth.draw();
    cloth.drawWire();
}
```

## Simulation Notes

- `ClothSettings::damping` controls damping. The damping is interpreted as the fraction of velocity removed over one second. The solver converts it to a per-step factor so changing `substeps` does not change the effective damping.
- `structuralStiffness`, `shearStiffness`, and `bendStiffness` are applied on each constraint iteration. Raising `constraintIterations` improves convergence and also increases effective stiffness, so examples tune stiffness and iteration count together.
- `ClothSettings::columns` and `rows` are clamped to `[2, 256]` during `setup()` to keep topology allocation bounded.
- Collider setters replace their full collider lists. Use `clearColliders()` before switching collision modes, or pass the complete current list to `setSphereColliders()` / `setPlaneColliders()`.

## Examples

- `examples/clothBasic`: GPU-first Auto, pinned cloth, gravity, light wind, fill and wire.
- `examples/clothCollision`: GPU-first Auto, animated sphere collider with full cloth interaction.
- `examples/clothWind`: GPU-first Auto, wider cloth with stronger visible wind.

From an example directory:

```bash
trusscli update
trusscli build
```

Compiled app bundles/binaries under example `bin/` or build folders are local
artifacts and are ignored by the repository.

## Known Limitations

- `draw()` still updates a TrussC `tc::Mesh` from CPU-visible positions; the
  solver is GPU-first, but rendering is not yet a pure GPU vertex-pull path.
- TexturePingPong supports Verlet integration, Jacobi distance constraints, wind,
  pinning, and the first sphere collider on the GPU. Additional collider arrays,
  planes, and ComputeStorageBuffer are future work.
- Cloth tearing, self-collision, and texture material support are not included yet.

## License Notes

tcxCloth is released under MIT by showlab. It was written from the public cloth
simulation concept, using Verlet integration and distance constraints, without
copying openFrameworks, glm, or OpenGL immediate-mode code.
