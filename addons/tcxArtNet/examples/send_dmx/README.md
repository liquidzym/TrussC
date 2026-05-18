# send_dmx

Sends one ArtDmx frame to a unicast Art-Net node.

Expected output:

```text
sent ArtDmx universe 0:0:1 to 127.0.0.1:6454
```

Network setup note: change the endpoint IP in `src/main.cpp` to your controller or LED processor.

Wireshark filter: `udp.port == 6454`
