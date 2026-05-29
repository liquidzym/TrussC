# tcxAssimp

TrussC addon for loading 3D assets through Assimp.

## Usage

Add `tcxAssimp` to an app's `addons.make`:

```text
tcxAssimp
```

Then use the public header:

```cpp
#include <tcxAssimp.h>

tcx::assimp::Model model;

void setup() {
    model.setScaleNormalize(true);
    model.load(tc::getDataPath("models/character.glb"));
    model.play(0);
}

void update() {
    model.update((float)tc::getDeltaTime());
}

void draw() {
    model.draw();
}
```

## Dependency Cache

Assimp is fetched and built by this addon, but the cache is shared across apps:

- source: `addons/tcxAssimp/_fcache/assimp-src`
- macOS build: `addons/tcxAssimp/build-macos/assimp-build`
- Windows build: `addons/tcxAssimp/build-windows/assimp-build`
- Linux build: `addons/tcxAssimp/build-linux/assimp-build`

The first build on each platform downloads and compiles Assimp. Later TrussC apps
that reference `tcxAssimp` reuse the same addon-level source and build
directories instead of creating a fresh `app/build-*/_deps/assimp-*` tree.

`UPDATE_DISCONNECTED` is enabled so an existing checkout does not require a
network update check on every configure.

## Current Scope

Implemented:

- OBJ / GLTF / GLB / FBX / DAE / STL / PLY import through Assimp
- scene nodes and mesh instances
- public `Node` / `Mesh` view API for hierarchy and asset inspection
- material color plus base-color, normal, metallic-roughness, emissive, occlusion texture binding
- glTF alpha mode / cutoff metadata import, with base alpha passed into TrussC material
- `TextureResolver` for external texture path resolution and GLB embedded texture upload
- bounding box and scale normalization
- animation clips, bind-pose-safe playback, loop/speed/time controls
- per-draw node transform reuse for lower CPU cost on multi-mesh models
- CPU skinning and skeleton debug draw
- addon-local GPU skinned renderer (`Model::setGpuSkinningEnabled(true)`)
- model-space bone global override API
- command-line verification example at `examples/verifyModelExample`
- task-focused app examples: static model, glTF material, FBX transform, FBX animation, bone control, scene hierarchy debug

Next required items:

- broader transform parity test corpus across GLB / FBX / DAE

GPU skinning is implemented inside this addon without changing TrussC core.
It uses a dedicated sokol shader and mesh buffer path for skinned meshes,
supports up to 128 bones per model, and falls back to CPU skinning when it is
not available. The GPU path supports material base color, base-color texture,
and alpha mask cutoff with simple directional lighting; full normal /
metallic-roughness / emissive / occlusion PBR parity remains a later TrussC
renderer/pipeline upgrade.

## Verification

Build and run the command-line verification example:

```bash
cd examples/verifyModelExample
cmake -S . -B build-macos
cmake --build build-macos
./build-macos/verifyModelExample
```

It verifies matrix conversion, animation bind-pose fallback, pause semantics,
`Model::setTransform`, OBJ loading, and optional Fox / FlightHelmet sample data
when those assets are present.
