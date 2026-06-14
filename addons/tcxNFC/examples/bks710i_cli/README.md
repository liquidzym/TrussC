# tcxNFC BKS-710I CLI

Hardware diagnostic CLI for a BKS-710I reader.

```bash
cmake -S . -B build-macos
cmake --build build-macos

./build-macos/tcxNFC_bks710i_cli ping --host 192.168.1.100
./build-macos/tcxNFC_bks710i_cli read-uid --host 192.168.1.100 --source-host 192.168.1.10
./build-macos/tcxNFC_bks710i_cli write-url --url https://wstree.cn/t/ABC123 --host 192.168.1.100
```
