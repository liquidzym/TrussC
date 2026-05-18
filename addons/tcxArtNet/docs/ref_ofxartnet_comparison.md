# ref/ofxArtnet Comparison

`ref/ofxArtnet` is useful as a compact ArtDmx-only reference. Its ArtDmx sender builds the standard 18-byte header, writes the opcode little-endian, writes the Port-Address low byte then high byte, and writes the DMX length high byte then low byte.

The reference does not implement ArtPollReply, ArtDiagData, ArtInput, ArtTimeCode, ArtTrigger, ArtIpProg, or the codec-only packet families, so it does not share the previous non-DMX field-offset bugs in `tcxArtNet`.

It does have related limitations:

- It only handles ArtDmx / OpOutput, not broader Art-Net 4 packets.
- The receiver uses a fixed `HEADER_LENGTH + 512` byte buffer, which is fine for ArtDmx but not enough for larger codec-only packet families.
- It depends on openFrameworks/ofxNetwork and is not a direct cross-platform TrussC addon pattern.
