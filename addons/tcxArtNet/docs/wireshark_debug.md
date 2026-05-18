# Wireshark Debugging

Use this display filter:

```text
udp.port == 6454
```

Useful checks:

- Art-Net ID starts with `41 72 74 2d 4e 65 74 00` (`Art-Net\0`).
- OpCode is little-endian, so ArtDmx appears as `00 50`.
- Protocol version bytes are `00 0e`.
- ArtDmx length is big-endian and must match the payload bytes.
- Port-address is little-endian at the ArtDmx universe field.
- `PacketInspector::bytesToHex()` prints the same byte order used on the wire.
- Non-DMX packets such as ArtInput, ArtTimeCode, ArtTrigger, ArtDiagData, and ArtIpProg include fixed filler bytes; compare against the official Art-Net packet definition instead of round-trip output only.
