# art_trigger_send_receive

Builds an ArtTrigger packet with a small macro payload.

Expected output:

```text
trigger key 1 subkey 2 payload 3
```

Network setup note: only send triggers to devices that document their key/subkey mapping.

Wireshark filter: `udp.port == 6454`
