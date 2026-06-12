# tcxSyphon

[Syphon](https://syphon.github.io/) GPU texture sharing for [TrussC](https://github.com/TrussC-org/TrussC) — publish and receive video frames between macOS applications, GPU-to-GPU, with no CPU readback.

> **macOS only.** Syphon is a macOS technology (shared `IOSurface`). On other platforms tcxSyphon compiles to a no-op stub so cross-platform projects still build. (Windows/Linux equivalent: Spout / a future `tcxSpout`.)

> ⚠️ **One-time setup on Xcode 26+:** building this addon requires the Metal compiler toolchain, which Apple now ships as a separate component. Run it once per machine:
> ```sh
> xcodebuild -downloadComponent MetalToolchain
> ```
> Without it the build fails at `default.metallib` with *"cannot execute tool 'metal' due to missing Metal Toolchain"*. See [Requirements](#requirements) for why. This affects **anyone building tcxSyphon** — it's not needed by TrussC itself or most other addons.

## What it does

Syphon shares a single GPU texture between processes. tcxSyphon wraps it in two classes that mirror Syphon's own vocabulary — the **publishing** side is a *Server*, the **receiving** side a *Client*:

```cpp
#include <tcxSyphon.h>
using namespace tcx;

// --- publish (server) ---
SyphonServer server;
server.setup("My Output");
// ... each frame:
server.publish(fbo);            // or publish(texture)

// --- receive (client) ---
SyphonClient client;
auto servers = SyphonClient::listServers();   // discover publishers
client.connect(servers.front());               // or connect("name", "appName")
// ... each frame:
client.receive(texture);        // texture is (re)allocated as BGRA8
if (client.isFrameNew()) { /* texture updated */ }
```

See `example-server/` and `example-client/` for runnable apps.

Syphon always shares **8-bit BGRA** frames (its `IOSurface` is fixed to `kCVPixelFormatType_32BGRA`). `SyphonClient::receive()` allocates the destination as `TextureFormat::BGRA8` so a same-format GPU blit is valid and the texture draws with correct colors. Senders may publish any format; Syphon converts to BGRA on its side.

## Requirements

- macOS, Metal (TrussC's default macOS backend).
- **Metal toolchain** (build time). The addon compiles Syphon's Metal shaders into a `default.metallib` and bundles it into your app. On Xcode 26+ the Metal toolchain is a separate component — install it once with:

  ```sh
  xcodebuild -downloadComponent MetalToolchain
  ```

  (TrussC core itself doesn't need this — it compiles shaders at runtime — but compiling Syphon's `.metal` offline does.)

## How it builds

This addon builds Syphon entirely **from source** (the `Syphon-Framework` git submodule) — no prebuilt binary:

- Only Syphon's **Metal + IPC** sources are compiled; the OpenGL backend is excluded.
- Syphon's `SyphonMetalShaders.metal` is compiled to `default.metallib` at build time and shipped into the app bundle's `Contents/Resources/` via TrussC's `tc_addon_bundle_file()` helper. `SyphonMetalServer` needs this metallib at runtime, or it fails to initialise.

Clone with submodules:

```sh
git clone --recursive <repo>
# or, after a plain clone:
git submodule update --init --recursive
```

## License

MIT (see `LICENSE`). Bundles the Syphon Framework (BSD-style) as a submodule.
