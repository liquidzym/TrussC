# tcxArtNet API Reference

## Namespace

All public API lives in `tcx::artnet`.

Use `#include <tcxArtNet.h>` in TrussC apps and examples. Internal protocol headers remain under `tcx/artnet/...` for focused advanced integrations.

## Core Types

- `Endpoint`: IPv4 address and UDP port.
- `UniverseAddress`: Art-Net 15-bit `Net + Sub-Net + Universe` address.
- `Error` / `ErrorCode`: optional error reporting for public APIs.
- `Packet`: `std::variant` of supported packet structs.
- `Statistics`: sent, received, invalid, unsupported, dropped, DMX, and discovery counters.

## Codec

`Codec::encode(packet, bytes, error)` serializes a packet to Art-Net wire bytes.

`Codec::decode(bytes, packet, error)` validates and decodes a UDP payload. Unknown opcodes and RDM opcodes produce `UnsupportedPacket`; malformed headers, short packets, bad protocol versions, invalid lengths, and truncated payloads return `false`.

## UDP

`UdpSocket` is an internal-style public utility for tests and low-level integrations. It supports open, bind, nonblocking mode, reuse-address, directed broadcast, `sendTo`, and `receiveFrom`.

## Sender

`Sender` is available at two levels. The low-level calls `sendPacket()`, `sendDmx(endpoint, universe, span)`, and `sendSync(endpoint)` emit explicit packets to explicit endpoints. The high-level DMX calls keep per-universe state in the sender:

- `setDestination()`, `addDestination()`, and `clearDestinations()` manage the default output targets.
- `setChannel()`, `setChannels()`, and `setColor()` update zero-based DMX slots in a full 512-channel universe buffer.
- `clear()` blacks out a universe while keeping it active; `removeUniverse()` drops it from future sends.
- `send()` emits every active universe to the configured destinations, and `startAutoSend(fps)` refreshes unchanged values from a background thread until `stopAutoSend()`.

Flat universe overloads use the Art-Net 15-bit port address `0..32767`. Channel numbers in the high-level API are zero-based: `0..511`.

## Receiver

`Receiver` still exposes raw packet callbacks through `setPacketCallback()`. It also caches incoming ArtDmx state so polling apps can read `getChannel()`, `getDmx()`, `getUniverses()`, and `hasUniverse()` without storing packets themselves. `hasNewData()` returns whether any ArtDmx frame arrived since the previous call, then clears that flag.

## Session

`Session` is the high-level facade for controller-style apps that need output, input, and discovery together. `SessionSettings` contains separate `SenderSettings`, `ReceiverSettings`, and `ControllerSettings` plus setup flags for each subsystem. `setup()` initializes the selected subsystems, `close()` closes all managed sockets, `recover()` rebuilds from the last settings, and `update()` advances controller discovery plus receiver polling when the receiver is not running on its own thread.

Use `sender()`, `receiver()`, and `controller()` to reach the underlying objects. `pollNodes()` and `getDiscoveredNodes()` forward the common discovery workflow. Defaults are chosen for a combined app: sender broadcast is enabled, receiver listens on UDP 6454, and controller binds an ephemeral local port to avoid a default same-port bind conflict. Configure the component settings directly when a site needs a stricter Art-Net port layout.

## Controller

`Controller` sends ArtPoll, ArtDmx, multi-universe ArtDmx, ArtSync, ArtAddress, ArtTrigger, and ArtTimeCode packets. `update()` polls incoming UDP packets and maintains a discovered node list from ArtPollReply packets. When `ControllerSettings::autoPoll` is enabled, `update()` also sends ArtPoll at `pollInterval` and prunes entries older than `pollTimeout`.

## Node

`Node` emulates an Art-Net node. It can answer ArtPoll with unicast ArtPollReply and exposes callbacks for ArtDmx, ArtNzs, ArtSync, ArtAddress, ArtInput, ArtTrigger, and ArtTimeCode. Configure `NodeSettings::ipAddress` and `bindIpAddress` so ArtPollReply advertises the virtual node correctly. With `enableIpProg`, ArtIpProg updates only the virtual node state and returns ArtIpProgReply; it does not change the host IP.

## DMX Runtime

`DmxReceiverState` is a lightweight per-universe cache. It accepts ArtDmx, can hold frames until ArtSync, rejects duplicate non-zero sequence numbers, and returns committed universe data through `getUniverseData()`.

## PixelMapper

`PixelMapper::splitPixelsToUniverses()` converts RGB/RGBW/GRB/GRBW/BGR/BGRA/RGBA byte streams into consecutive ArtDmx frames. RGB defaults to 510 channels per universe. RGBW/RGBA/BGRA/GRBW formats default to 512 channels when the caller leaves `channelsPerUniverse` at the default value. `PixelToDmxOptions` also supports `brightness`, `gamma`, and RGBW/GRBW white extraction.

## PacketInspector

`PacketInspector::summarize()`, `bytesToHex()`, and `decodeHexBytes()` provide small diagnostics helpers for examples, logs, and Wireshark-oriented debugging.
