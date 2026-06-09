# tcxArtNet

[![Build](https://github.com/TrussC-org/TrussC/actions/workflows/build.yml/badge.svg)](https://github.com/TrussC-org/TrussC/actions/workflows/build.yml)

`tcxArtNet` is a TrussC C++20 addon for Art-Net 4 packet encoding, decoding, UDP transport, controller workflows, node emulation, and LED pixel-to-DMX splitting.

It has no openFrameworks dependency and does not use Boost, asio, Poco, libartnet, sACN, or RDM libraries. The addon implements core Art-Net protocol packets directly and keeps RDM, firmware flashing, file transfer, video, and media-server business logic behind codec-only extension points.

Interop notes: the ArtDmx sender/receiver paths use standard UDP 6454 and can be checked with lighting controllers such as QLC+ over loopback or a fixture network. The headless addon tests cover packet codec behavior plus local Sender/Receiver loopback paths; physical fixture and hardware Art-Net node coverage depends on the target network and device, so use the diagnostics examples when checking a real lighting rig.

## Include

Use the addon like the other source-based TrussC addons:

```cpp
#include <tcxArtNet.h>
```

Detailed protocol headers live under `src/tcx/artnet/` and are included by the umbrella header.

## Minimal Sender

```cpp
#include <tcxArtNet.h>

tcx::artnet::Sender sender;
tcx::artnet::Error error;
sender.setup(false, &error);
sender.setDestination({ "192.168.1.50", tcx::artnet::DefaultPort });

sender.setChannel(1, 0, 255);                         // universe 1, channel 0
sender.setColor(1, 3, trussc::Color(0.0f, 0.2f, 1.0f)); // channels 3, 4, 5
sender.startAutoSend(30.0, &error);                    // keep refreshing DMX
```

High-level Sender channel APIs are zero-based: channel `0` is the first DMX slot and channel `511` is the last. Fixture manuals and lighting consoles usually label those same slots as `1..512`, so subtract one when copying addresses from a fixture profile. Low-level APIs such as `sendDmx(endpoint, universe, span)` send the span exactly as provided.

## Integrated Session

Use `Session` when one app needs normal DMX output, ArtDmx input, and node discovery at the same time:

```cpp
tcx::artnet::Session artnet;
tcx::artnet::SessionSettings settings;
settings.receiver.port = tcx::artnet::DefaultPort;      // listen for ArtDmx
settings.controller.directedBroadcastIp = "2.255.255.255";
artnet.setup(settings);

artnet.sender().setDestination({ "192.168.1.50", tcx::artnet::DefaultPort });
artnet.sender().setColor(0, 0, trussc::Color(1.0f, 0.2f, 0.0f));
artnet.pollNodes();

while (running) {
    artnet.update(); // polls receiver + controller discovery
    if (artnet.receiver().hasNewData()) {
        uint8_t first = artnet.receiver().getChannel(0, 0);
    }
}
```

`Session` composes `Sender`, `Receiver`, and `Controller`; it does not change `Node`, which remains a node-emulation surface. By default, the session sender enables broadcast and the controller uses an ephemeral local port to avoid colliding with the receiver's default UDP 6454 bind. If a real site requires ArtPoll from UDP 6454, configure `SessionSettings::controller.localPort` explicitly and disable or move the receiver if the platform does not allow sharing that port.

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

- `Session` is a high-level facade for apps that need output, input, and discovery in one object. It exposes `sender()`, `receiver()`, `controller()`, `pollNodes()`, `update()`, and `getDiscoveredNodes()`.
- `Sender` supports state-oriented DMX output: `setChannel()`, `setChannels()`, `setColor()`, `clear()`, `removeUniverse()`, `send()`, and `startAutoSend()` for continuous refresh. It sends full 512-channel universes from the high-level API.
- `Receiver` can cache the latest ArtDmx data per universe with `getChannel()`, `getDmx()`, `getUniverses()`, `hasUniverse()`, and `hasNewData()`. `hasNewData()` clears when read, so an app can skip work on frames where DMX did not change.
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
