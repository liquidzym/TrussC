# video_packet_decode

Demonstrates codec-only ArtVideoData payload handling.

Expected output:

```text
decoded ArtVideoData payload bytes: 4
```

Network setup note: video packets are not rendered by the addon; decode them for inspection or forwarding.

Wireshark filter: `udp.port == 6454`
