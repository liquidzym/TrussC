# Network Setup

Art-Net uses UDP port `6454`. For a controller sending to real fixtures or LED processors, prefer unicast to the target node IP. Use directed broadcast only for discovery and ArtSync workflows.

Recommended defaults:

- Controller local bind: `0.0.0.0:6454`
- ArtPoll directed broadcast: `2.255.255.255` or the broadcast address for your interface subnet
- ArtDmx: unicast to each node
- ArtSync: directed broadcast after sending all universe frames
- Wireshark filter: `udp.port == 6454`

Avoid limited broadcast `255.255.255.255` unless your network explicitly requires it. Many Art-Net installations use `2.x.x.x` or `10.x.x.x` private lighting networks; match the fixture/controller manual and your NIC configuration.
