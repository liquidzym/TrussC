# tcxArtNet Protocol Scope

`tcxArtNet` aims to cover Art-Net 4 packet structure, validation, dispatch, and the common high-level flows used by TrussC applications: ArtPoll discovery, ArtPollReply node listing, ArtDmx output/input, ArtNzs, ArtSync, node emulation, diagnostics, timecode, trigger, ArtAddress, ArtInput, and virtual IP programming packets.

The addon intentionally does not implement RDM behavior, RDM discovery, RDM parameter parsing, RDM UID management, RDMnet, sACN, or E1.31. `ArtRdm` and `ArtRdmSub` opcodes are recognized and surfaced as unsupported RDM packets without payload parsing.

Advanced Art-Net packet families such as media, video, firmware, file transfer, directory, and TOD are represented as codecs with payload storage. They are available for packet inspection, forwarding, and future extension, but the addon does not perform real firmware flashing, file transfer, media-server control, video playback, or RDM discovery.
