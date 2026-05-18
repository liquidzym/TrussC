# diagnostics_monitor

Decodes ArtDiagData and prints priority plus message.

Expected output:

```text
diag priority 128: lamp online
```

Network setup note: enable diagnostics in the controller or node before expecting ArtDiagData traffic.

Wireshark filter: `udp.port == 6454`
