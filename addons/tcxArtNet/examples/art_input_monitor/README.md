# art_input_monitor

Decodes an ArtInput packet and prints its port state.

Expected output:

```text
decoded ArtInput ports: 4
```

Network setup note: use this pattern inside a receiver callback to monitor real ArtInput packets.

Wireshark filter: `udp.port == 6454`
