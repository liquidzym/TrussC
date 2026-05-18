# ip_prog_tool

Builds an ArtIpProg packet without changing the host machine IP.

Expected output:

```text
encoded ArtIpProg for 10.0.0.50
```

Network setup note: hardware IP programming should only be enabled by an application-level explicit user action.

Wireshark filter: `udp.port == 6454`
