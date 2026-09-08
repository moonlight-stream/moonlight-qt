# GilStreaming coordinator prototype

This dependency-free Go service is the pool authority for the two-VM MVP. It
supports authenticated lease creation, idempotent recovery, heartbeats, release,
expiry, and crash-safe JSON state persistence.

It does not yet perform VM health checks, cleanup, or Sunshine pairing. Keep it
on a private network until TLS is terminated by a trusted reverse proxy.

```powershell
Copy-Item config.example.json config.json
go run . -hash-token "choose-a-long-random-token"
# Put the printed hash in config.json, then:
go run . -config config.json
```

Run tests with `go test ./...`.
