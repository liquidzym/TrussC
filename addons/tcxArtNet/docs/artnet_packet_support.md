# Art-Net Packet Support

| Packet | Opcode | Codec | High-Level Behavior | Notes |
|---|---:|---|---|---|
| ArtPoll | `0x2000` | Yes | Controller send, Node reply handling | Target fields supported |
| ArtPollReply | `0x2100` | Yes | Controller discovery list, Node unicast reply | Decodes 207-byte minimum |
| ArtDiagData | `0x2300` | Yes | Node diagnostics send | Priority and logical port stored |
| ArtCommand | `0x2400` | Yes | Codec/callback-ready | No device-specific command table |
| ArtDataRequest | `0x2700` | Yes | Codec/callback-ready | No URL downloader |
| ArtDataReply | `0x2800` | Yes | Codec/callback-ready | Raw data payload |
| ArtDmx / ArtOutput | `0x5000` | Yes | Controller send, Node callback | 2-512 even length |
| ArtNzs | `0x5100` | Yes | Node callback | RDM start code rejected |
| ArtSync | `0x5200` | Yes | Controller send, Node callback | Directed broadcast workflow |
| ArtAddress | `0x6000` | Yes | Controller send, Node callback | RDM/sACN commands are not executed |
| ArtInput | `0x7000` | Yes | Codec/callback-ready | DMX input state only |
| ArtTodRequest | `0x8000` | Yes | Codec only | No RDM discovery |
| ArtTodData | `0x8100` | Yes | Codec only | No RDM discovery |
| ArtTodControl | `0x8200` | Yes | Codec only | No RDM discovery |
| ArtRdm | `0x8300` | Recognized | Unsupported | Intentionally not implemented |
| ArtRdmSub | `0x8400` | Recognized | Unsupported | Intentionally not implemented |
| ArtMedia | `0x9000` | Yes | Codec only | No media-server business logic |
| ArtMediaPatch | `0x9100` | Yes | Codec only | No media-server business logic |
| ArtMediaControl | `0x9200` | Yes | Codec only | No media-server business logic |
| ArtMediaControlReply | `0x9300` | Yes | Codec only | No media-server business logic |
| ArtTimeCode | `0x9700` | Yes | Controller send, Node callback | Stream ID supported |
| ArtTimeSync | `0x9800` | Yes | Codec | Does not alter system time |
| ArtTrigger | `0x9900` | Yes | Controller send, Node callback | Fixed 512-byte wire payload |
| ArtDirectory | `0x9a00` | Yes | Codec only | Raw payload |
| ArtDirectoryReply | `0x9b00` | Yes | Codec only | Raw payload |
| ArtVideoSetup | `0xa010` | Yes | Codec only | No video playback |
| ArtVideoPalette | `0xa020` | Yes | Codec only | No video playback |
| ArtVideoData | `0xa040` | Yes | Codec only | No video playback |
| ArtFirmwareMaster | `0xf200` | Yes | Codec only | No firmware flashing |
| ArtFirmwareReply | `0xf300` | Yes | Codec only | No firmware flashing |
| ArtFileTnMaster | `0xf400` | Yes | Codec only | No real transfer |
| ArtFileFnMaster | `0xf500` | Yes | Codec only | No real transfer |
| ArtFileFnReply | `0xf600` | Yes | Codec only | No real transfer |
| ArtIpProg | `0xf800` | Yes | Controller send / virtual handling | Does not modify host IP |
| ArtIpProgReply | `0xf900` | Yes | Codec / virtual node reply | Virtual IP state only |

ArtRdm, ArtRdmSub, sACN, and E1.31 are intentionally not implemented.
