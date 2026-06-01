# tcxCloth GPU Notes

Updated: 2026-06-01

This note records the current GPU-first implementation state, the main fixes
made while validating the examples, and the practical constraints to keep in
mind when continuing this addon.

## Current Status

- The examples use `ClothSettings::SolverBackend::Auto`.
- `Auto` prefers `TexturePingPong` when a graphics context and RGBA32F render
  targets are available.
- If GPU setup or readback fails, the solver falls back to `CpuReference`.
- `examples/clothBasic` shows pinned-cloth sag and wind on the GPU. A larger
  window shows the full cloth surface.
- `examples/clothCollision` now keeps the sphere close enough to the cloth
  plane that back-side pushing produces a clear dome and edge displacement.
- `examples/clothWind` uses the same GPU position backend and remains the
  strongest wind/billow demo.

## GPU Solver Shape

The `TexturePingPong` backend stores cloth state in RGBA32F FBOs:

- `position[2]`: current particle positions.
- `previous[2]`: previous positions for Verlet velocity.
- `pinMask`: dynamic RGBA32F texture where `r` is pinned state and `gba` is the
  pinned position.

The shader pass modes are:

- `0`: reset to rest or pinned positions.
- `1`: Verlet integration with gravity, global force, and wind.
- `2`: Jacobi distance constraints.
- `3`: sphere collision.
- `4`: copy current position into previous.

Each GPU step runs:

1. Constraint pass before integration, matching the CPU solver's pre-solve.
2. Verlet integration.
3. Copy old current position into `previous`.
4. Constraint pass after integration.
5. Sphere collision pass.
6. CPU readback into the current TrussC `tc::Mesh` render path.

## Important Fixes From Validation

- macOS Metal float FBO readback must use texture-to-buffer blit with 256-byte
  row alignment. Texture-to-texture staging returned zeroes for the RGBA32F
  readback path and made the GPU solver look like it was not running.
- The fullscreen pass pipeline must match the RGBA32F FBO color format and
  depth-stencil attachment.
- GPU constraints should use an explicit relaxation value (`0.30f` currently).
  Averaging by neighbor count made the cloth too soft; accumulating full
  neighbor corrections without relaxation was numerically unstable.
- Keep collision pass state and readback synchronized. Collision only modifying
  the position texture is fine for a visible contact response, but future
  stronger collision impulse work should also consider how `previous` is updated
  to control rebound and sliding.

## Collision Example Notes

`clothCollision` is a visual demo, not just a raw physics test. The sphere now
uses a larger radius and a z path that keeps it near the cloth plane:

```cpp
sphere_.radius = 76.0f;
sphere_.center.z = -42.0f + 88.0f * (0.5f + 0.5f * std::sin(t * 0.9f));
```

This matters because a true 3D sphere only pushes vertices that are within its
radius. If the sphere is visually behind the cloth but physically too far in z,
the cloth will barely move. For "ball behind the fabric" demos, keep the sphere
center within roughly one radius of the cloth surface, or add a dedicated
projected/back-side support force.

## Remaining Wrap-Up Items

- Update `screenshot.png` once the final preferred demo framing is chosen.
- Validate Windows D3D11 and Web builds; current visual verification has been
  on macOS Metal.
- Replace CPU readback mesh rendering with a true GPU render path when TrussC
  has the needed vertex-pull or storage-buffer path.
- Extend GPU collision beyond the first sphere collider if multiple colliders
  are needed.
- Add plane colliders to the GPU path if the examples start relying on them.

## Do Not Regress

- Do not change examples back to `CpuReference`; they should remain GPU-first
  through `Auto`.
- Do not reintroduce temporary `cout` output. Use `tc::logNotice` sparingly for
  real diagnostics.
- Do not leave per-frame diagnostic logs in `Cloth.cpp`.
- Do not remove the macOS readback row-alignment fix unless another verified
  readback path replaces it.
- Do not tune collision only by eye without checking that the sphere is
  physically close enough to the cloth plane.
