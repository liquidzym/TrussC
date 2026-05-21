# tcxArtNet

`tcxArtNet` is a TrussC C++20 addon for Art-Net 4 packet encoding, decoding, UDP transport, controller workflows, node emulation, and LED pixel-to-DMX splitting.

It has no openFrameworks dependency and does not use Boost, asio, Poco, libartnet, sACN, or RDM libraries. The addon implements core Art-Net protocol packets directly and keeps RDM, firmware flashing, file transfer, video, and media-server business logic behind codec-only extension points.

## Include

Use the addon like the other source-based TrussC addons:

```cpp
#include <tcxArtNet.h>
```

Detailed protocol headers live under `src/tcx/artnet/` and are included by the umbrella header.

## Minimal Sender

```cpp
#include <tcxArtNet.h>

tcx::artnet::Controller controller;
tcx::artnet::ControllerSettings settings;
controller.setup(settings);

std::array<uint8_t, 6> dmx { 255, 0, 0, 0, 255, 0 };
tcx::artnet::Endpoint node { "192.168.1.50", tcx::artnet::DefaultPort };
tcx::artnet::UniverseAddress universe { 0, 0, 1 };
controller.sendDmx(node, universe, dmx);
```

## Minimal Node

```cpp
tcx::artnet::Node node;
tcx::artnet::NodeSettings settings;
settings.outputPorts.push_back({ tcx::artnet::UniverseAddress { 0, 0, 1 }, false, true, true });
node.setup(settings);
node.setDmxOutputCallback([](const tcx::artnet::ArtDmx& dmx) {
    // Copy dmx.data to a fixture, LED driver, or simulator.
});

while (running) {
    node.update();
}
```

## Runtime Helpers

- `Controller` can auto-poll and prune stale discovered nodes using `ControllerSettings::autoPoll`, `pollInterval`, and `pollTimeout`.
- `Controller::networkDiagnostics()` reports the requested bind IP/port, actual local endpoint, last target endpoint, recent socket error, and recovery state. `Controller::recover()` rebuilds the socket from the last `ControllerSettings`.
- `UdpSocket::diagnostics()` reports actual bind results after binding to a specific local interface or an ephemeral port.
- `makeDirectedBroadcastIp()` and `makeDirectedBroadcastEndpoint()` construct subnet-aware broadcast targets without app-side `/24` string guessing.
- `probeUdpOutput()` provides a reusable startup/site probe for checking whether a bind IP can send UDP to an Art-Net target.
- `Node` can advertise configured IP/bind IP information, apply virtual `ArtAddress` name updates, and answer virtual `ArtIpProg` packets without changing the host network interface.
- `DmxReceiverState` stores per-universe DMX frames, rejects duplicate non-zero sequences, and can buffer frames until `ArtSync`.
- `PixelMapper` supports brightness, gamma, and RGBW/GRBW white extraction.
- `PacketInspector` provides packet summaries, hex dumps, and hex byte parsing for diagnostics.

## Examples

The `examples/` directory contains sender, receiver, discovery, node emulation, ArtSync, pixel mapping, address/input control, timecode, trigger, diagnostics, IP programming, packet inspection, codec inspection, and CLI site-probe tools. Each example README includes expected output, a network setup note, and the Wireshark filter `udp.port == 6454`.

## Tests

```bash
cd addons/tcxArtNet/tests
cmake -S . -B build-macos
cmake --build build-macos --target tcxArtNet_tests -j4
./build-macos/tcxArtNet_tests
```

## Intentional Non-Scope

`ArtRdm`, `ArtRdmSub`, RDM discovery, RDM parameter parsing, RDM UID management, RDMnet, sACN, and E1.31 are intentionally not implemented.

## Pixel Formats

`PixelMapper` defaults to RGB with 510 channels per universe. It also supports RGBW, GRB, GRBW, BGR, BGRA, and RGBA. Four-channel formats use 512 channels per universe when the caller leaves `channelsPerUniverse` at the default value, which matches common RGBA/RGBW fixtures.
