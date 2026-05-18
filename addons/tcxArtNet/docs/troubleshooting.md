# Troubleshooting

## No Packets Received

Check that the receiver is bound to UDP port `6454`, the OS firewall permits UDP traffic, and the sending endpoint uses the expected network interface.

## Discovery Works But DMX Does Not

Discovery often uses broadcast while DMX is best sent unicast. Confirm the ArtPollReply IP address and send ArtDmx to that IP. Check the universe address: `Net`, `Sub-Net`, and `Universe` must match the fixture or LED processor.

## Odd ArtDmx Length Rejected

ArtDmx payload length must be even and in the range `2..512`. Use `PixelMapper` for RGB/RGBW LED data; it pads final odd-length RGB fragments to preserve valid ArtDmx packets.

## RDM Packet Unsupported

This addon intentionally does not implement ArtRdm or ArtRdmSub. RDM opcodes are recognized and surfaced as unsupported packets without parsing the payload.
