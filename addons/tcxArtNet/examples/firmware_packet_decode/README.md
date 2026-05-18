# firmware_packet_decode

Demonstrates codec-only ArtFirmwareReply payload handling.

Expected output:

```text
decoded ArtFirmwareReply payload bytes: 2
```

Network setup note: firmware packets are decoded only; firmware flashing is intentionally not implemented.

Wireshark filter: `udp.port == 6454`
