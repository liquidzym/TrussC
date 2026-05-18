# packet_inspector

Builds an ArtDmx packet, prints a compact packet summary, and emits a hex dump that can be pasted into packet debugging tools.

Expected output includes `ArtDmx universe 1/2/3` and the Art-Net header bytes `41 72 74 2d 4e 65 74 00`.

Network setup note: this example does not open UDP sockets.
