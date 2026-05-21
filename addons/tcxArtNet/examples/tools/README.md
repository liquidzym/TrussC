# tcxArtNet Tools

Small CLI probes for现场 Art-Net checks.

- `artnet_socket_probe`: opens a UDP socket, binds to a requested local IP, sends a minimal Art-Net-shaped payload, and prints the actual bound endpoint.
- `artnet_output_probe`: sends three ArtDmx frames through `tcx::artnet::Controller` and prints target, bind, packet, and recovery diagnostics.

Build:

```sh
cmake -S examples/tools -B examples/tools/build-macos
cmake --build examples/tools/build-macos -j4
```

Run examples:

```sh
examples/tools/build-macos/artnet_socket_probe 192.168.1.100 192.168.1.10
examples/tools/build-macos/artnet_output_probe 192.168.1.10 192.168.1.100
```

Pass `broadcast` as the third argument to either tool when checking directed-broadcast routing.
