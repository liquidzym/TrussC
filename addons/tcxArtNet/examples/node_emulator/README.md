# node_emulator

Creates a virtual Art-Net node with one output port and answers ArtPoll with ArtPollReply.

Expected output:

```text
tcxArtNet virtual node listening on UDP 6454
```

Network setup note: run this on the same subnet as your Art-Net controller.

Wireshark filter: `udp.port == 6454`
