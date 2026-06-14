# tcxNFC Architecture

## Scope

`tcxNFC` is an addon boundary, not a single exhibition app. The app owns UI, persistence, LED effects, and deployment policy. The addon owns NFC/RFID protocol helpers, reader abstractions, token provider boundaries, and plugin dispatch.

## Layers

```text
TrussC app / headless process
  |
  | calls
  v
ActivationRuntime
  |
  |-- IReader
  |     |-- Bks710iReader
  |           |-- PrivateProtocolClient -> TCP 0x50 UID
  |           |-- ModbusTcpClient       -> 0x62 write / 0x61 readback
  |
  |-- ITokenProvider
  |     |-- CloudTokenProvider with injected ICloudTokenClient
  |     |-- FixedUrlTokenProvider
  |     |-- FallbackTokenProvider
  |
  |-- PluginHost -> IActivationPlugin::onActivation()
```

## Hardware Path

The BKS-710I path is based on the verified `mayaRFID` implementation and reference documents:

- Reader default IP and port: `192.168.1.100:502`.
- Private UID command: `0x50`.
- Modbus NTAG fast read: `0x61`.
- Modbus NTAG write: `0x62`.
- NTAG user data starts at page `4`.

The default write path builds a Type 2 Tag NDEF URI TLV:

```text
03 LL D1 01 PL 55 04 <https suffix> FE
```

The bytes are padded to 4-byte NTAG pages before building `0x62` registers. `Bks710iReader::writeUrlRawNtag()` only reports `verified` after the readback bytes match.

## Cloud Boundary

The addon intentionally does not hard-code HTTP. Production apps can inject an `ICloudTokenClient` backed by REST, MQTT, local IPC, or a queued sync service. `FallbackTokenProvider` preserves the exhibition behavior from `mayaRFID`: try the online provider first, then fall back to a fixed/preloaded URL when offline.

## Plugin Boundary

`PluginHost` is intentionally small: it dispatches completed `ActivationEvent` objects to registered plugins. A TrussC app can attach plugins for LEDs, local logging, SQLite queueing, analytics upload, or UI updates without coupling those systems to NFC protocol code.
