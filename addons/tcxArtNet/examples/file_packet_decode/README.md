# file_packet_decode

Demonstrates codec-only ArtFileFnReply payload handling.

Expected output:

```text
decoded ArtFileFnReply payload bytes: 3
```

Network setup note: file packets are not uploaded or downloaded by the addon core.

Wireshark filter: `udp.port == 6454`
