# media_packet_decode

Demonstrates codec-only ArtMedia payload handling.

Expected output:

```text
decoded ArtMedia payload bytes: 4
```

Network setup note: media packets are device-specific; this example only validates payload decode.

Wireshark filter: `udp.port == 6454`
