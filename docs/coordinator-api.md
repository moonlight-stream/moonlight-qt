# Coordinator API v1

All endpoints use HTTPS and JSON. Client endpoints require
`Authorization: Bearer <token>`. Lease IDs are opaque random values.

## Start GILid login

`POST /v1/auth/start`

The response contains a one-time `requestId` and GILid `authorizeUrl`. The
desktop opens the URL in the system browser and polls
`GET /v1/auth/status/{requestId}`. The registered redirect URI points to
`GET /auth/callback` on this coordinator, where the server performs the
confidential token exchange.

Once complete, the status endpoint returns a GilStreaming access token and
basic profile. It can return that token only once.

Debug clients may call `POST /v1/auth/dev` when `devAuthEnabled` is explicitly
enabled on the coordinator. This endpoint must be disabled in production.

## Create or recover a lease

`POST /v1/leases`

```json
{
  "deviceId": "stable-client-installation-id",
  "deviceName": "Gil's laptop"
}
```

The operation is idempotent for a user/device with an active lease. Success:

```json
{
  "leaseId": "opaque-random-id",
  "state": "reserved",
  "expiresAt": "2026-09-07T20:10:00Z",
  "host": {
    "name": "Gaming VM 1",
    "address": "vm1.gilstreaming.internal",
    "port": 47989
  },
  "pairingRequired": true
}
```

If both VMs are busy, return `409` with `code: "POOL_EXHAUSTED"` and a retry
hint. Never return the full VM inventory to a client.

## Renew a lease

`POST /v1/leases/{leaseId}/heartbeat`

The client sends this periodically while it owns the session. The response
contains the renewed `expiresAt`. A missing, expired, or revoked lease returns
`404` or `410` and the client must not start another stream on that endpoint.

## Pair the assigned client

`POST /v1/leases/{leaseId}/pair`

```json
{
  "pin": "1234",
  "deviceName": "Gil's laptop"
}
```

The coordinator verifies lease ownership and submits the PIN only to the
assigned VM. It discovers the pending pairing ID when required by current
Sunshine releases and also supports the legacy PIN endpoint. Sunshine
credentials never appear in this response.

## Release a lease

`DELETE /v1/leases/{leaseId}`

The coordinator immediately moves the VM to `cleaning`. This endpoint is
idempotent. A background reaper performs the same transition for expired
leases, so a crashed or disconnected client cannot occupy a slot forever.

## Administrative VM records

The coordinator keeps these fields for each VM:

```json
{
  "id": "vm-1",
  "displayName": "Gaming VM 1",
  "discoveryName": "v1",
  "streamAddress": "192.168.1.23",
  "streamPort": 47989,
  "publicAddress": "stream.gilservers.com",
  "publicStreamPort": 47989,
  "sunshineApiUrl": "https://192.168.1.23:47990",
  "enabled": true
}
```

`discoveryName` is the stable, unique Windows/Sunshine hostname advertised over
mDNS. The coordinator refreshes the private stream and management IP addresses
from `_nvstream._tcp.local` before creating or recovering a lease. The explicit
private addresses are retained as a fallback. Only `publicAddress` and
`publicStreamPort` are returned to the client when they are configured.

All VMs use the coordinator-only `SUNSHINE_USERNAME` and `SUNSHINE_PASSWORD`.
Sunshine credentials are never fields returned by an API.
Health, lease owner, lease expiry, and cleanup state are coordinator-owned.
