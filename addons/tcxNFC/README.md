# tcxNFC

`tcxNFC` is a TrussC addon for NFC/RFID activation systems. It packages the verified `mayaRFID` hardware path into reusable addon layers:

- NFC Forum Type 2 Tag HTTPS URI TLV generation.
- BKS-710I private TCP `0x50` UID request frames.
- BKS-710I Modbus TCP `0x62` NTAG URL write frames and `0x61` readback verification flow.
- Reader, cloud token, fallback token, activation runtime, and plugin dispatch interfaces.
- Examples under `examples/` only.

## Include

```cpp
#include <tcxNFC.h>
```

## Minimal Runtime

```cpp
tcx::nfc::TcpEndpoint endpoint;
endpoint.host = "192.168.1.100";
endpoint.port = 502;
endpoint.sourceHost = "192.168.1.10"; // optional direct Ethernet bind

tcx::nfc::TcpSocketTransport privateTcp(endpoint);
tcx::nfc::TcpSocketTransport modbusTcp(endpoint);
tcx::nfc::Bks710iReader reader(privateTcp, modbusTcp);

tcx::nfc::FixedUrlTokenProvider fallback("https://wstree.cn/t/FALLBACK");

tcx::nfc::ActivationConfig config;
config.deviceId = "pi01";
config.readerId = "bks710i-main";

auto result = tcx::nfc::ActivationRuntime::runOnce(config, reader, fallback);
```

## Examples

```bash
cmake -S examples/ndef_cli -B examples/ndef_cli/build-macos
cmake --build examples/ndef_cli/build-macos
./examples/ndef_cli/build-macos/tcxNFC_ndef_cli https://wstree.cn/t/ABC123

cmake -S examples/plugin_runtime_mock -B examples/plugin_runtime_mock/build-macos
cmake --build examples/plugin_runtime_mock/build-macos
./examples/plugin_runtime_mock/build-macos/tcxNFC_plugin_runtime_mock

cmake -S examples/bks710i_cli -B examples/bks710i_cli/build-macos
cmake --build examples/bks710i_cli/build-macos
./examples/bks710i_cli/build-macos/tcxNFC_bks710i_cli read-uid --host 192.168.1.100 --source-host 192.168.1.10

cd examples/trussc_activation_mock
trusscli update -p .
cmake --preset macos
trusscli build
./bin/trussc_activation_mock.app/Contents/MacOS/trussc_activation_mock
```

Cloud mock:

```bash
node examples/cloud_mock/server.js
```

## Tests

```bash
cmake -S tests -B tests/build-macos
cmake --build tests/build-macos
./tests/build-macos/tcxNFC_tests
```

## Reference Basis

This addon uses the practical path validated in `/Users/mac/Desktop/VIBECODING/TrussC/mayaRFID`:

- Default BKS-710I reader endpoint: `192.168.1.100:502`.
- UID read: private TCP command `0x50`.
- NTAG URL write: Modbus TCP command `0x62` from page 4.
- Verification: read written pages back with `0x61` before claiming success.
