# led_pixels_to_artnet

Maps RGBW LED bytes into ArtDmx universes using `PixelMapper`, white extraction, and brightness scaling.

Expected output:

```text
mapped RGBW pixels with white extraction to 1 ArtDmx frames
```

Network setup note: use GRB, RGBW, RGBA, BGRA, or GRBW formats when your fixture expects a different channel order. Use `extractWhite` only when RGBW/GRBW fixtures should move shared RGB intensity into white.

Wireshark filter: `udp.port == 6454`
