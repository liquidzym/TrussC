# art_trigger_send_receive

Builds an ArtTrigger packet with a small macro payload. The Art-Net wire packet always carries a 512-byte payload field, so decoded payload size is fixed.

Expected output:

```text
trigger key 1 subkey 2 payload 512
```

Network setup note: only send triggers to devices that document their key/subkey mapping.

Wireshark filter: `udp.port == 6454`
