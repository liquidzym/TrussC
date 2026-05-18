# art_timecode_send_receive

Encodes and sends ArtTimeCode, then decodes it locally to show the receive-side API.

Expected output:

```text
timecode 01:02:03:04 stream 1
```

Network setup note: set the endpoint to a node or media server that consumes ArtTimeCode.

Wireshark filter: `udp.port == 6454`
