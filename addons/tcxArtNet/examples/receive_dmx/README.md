# receive_dmx

Polls UDP port `6454` for ArtDmx packets and prints received frames.

Expected output:

```text
waiting for ArtDmx on UDP 6454
```

Network setup note: only one process can bind `6454` on some systems. Stop other Art-Net receivers first.

Wireshark filter: `udp.port == 6454`
