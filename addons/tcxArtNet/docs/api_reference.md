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

## Controller

`Controller` sends ArtPoll, ArtDmx, multi-universe ArtDmx, ArtSync, ArtAddress, ArtTrigger, and ArtTimeCode packets. `update()` polls incoming UDP packets and maintains a discovered node list from ArtPollReply packets.

## Node

`Node` emulates an Art-Net node. It can answer ArtPoll with unicast ArtPollReply and exposes callbacks for ArtDmx, ArtNzs, ArtSync, ArtAddress, ArtTrigger, and ArtTimeCode.

## PixelMapper

`PixelMapper::splitPixelsToUniverses()` converts RGB/RGBW/GRB/GRBW/BGR/BGRA/RGBA byte streams into consecutive ArtDmx frames. RGB defaults to 510 channels per universe. RGBW/RGBA/BGRA/GRBW formats default to 512 channels when the caller leaves `channelsPerUniverse` at the default value.
