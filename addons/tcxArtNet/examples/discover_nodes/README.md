# discover_nodes

Sends ArtPoll to a directed broadcast address and prints the current discovery list after one update pass.

Expected output:

```text
poll sent
nodes discovered: 0
```

Network setup note: change `directedBroadcastIp` in `src/main.cpp` to your interface broadcast address.

Wireshark filter: `udp.port == 6454`
