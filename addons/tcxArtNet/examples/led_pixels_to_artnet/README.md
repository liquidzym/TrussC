# led_pixels_to_artnet

Maps RGB LED bytes into ArtDmx universes using `PixelMapper`.

Expected output:

```text
mapped RGB pixels to 2 ArtDmx frames
```

Network setup note: use GRB, RGBW, RGBA, BGRA, or GRBW formats when your fixture expects a different channel order.

Wireshark filter: `udp.port == 6454`
