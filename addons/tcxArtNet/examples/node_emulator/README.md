# node_emulator

Creates a virtual Art-Net node with one output port, configured PollReply IP fields, virtual ArtAddress renaming, and virtual ArtIpProg replies.

Expected output:

```text
tcxArtNet virtual node listening on UDP 6454 at 2.0.0.20
ArtAddress renaming and virtual ArtIpProg replies are enabled
```

Network setup note: run this on the same subnet as your Art-Net controller.

Wireshark filter: `udp.port == 6454`
