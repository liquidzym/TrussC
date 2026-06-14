# tcxNFC Cloud Mock

Small dependency-free HTTP server for integration tests and demos.

```bash
node server.js
curl -X POST http://127.0.0.1:8787/api/token \
  -H 'content-type: application/json' \
  -d '{"uid":"04:A1:B2:C3","deviceId":"pi01","readerId":"bks710i-main"}'
```
