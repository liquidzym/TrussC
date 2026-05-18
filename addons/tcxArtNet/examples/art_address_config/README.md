# art_address_config

Builds and sends an ArtAddress packet for remote node configuration.

Expected output:

```text
sent ArtAddress config packet
```

Network setup note: point the endpoint at a node that explicitly allows remote ArtAddress changes.

Wireshark filter: `udp.port == 6454`
