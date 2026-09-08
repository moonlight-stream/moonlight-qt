# Coordinator API v1

All endpoints use HTTPS and JSON. Client endpoints require
`Authorization: Bearer <token>`. Lease IDs are opaque random values.

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
  "clientName": "GilStreaming - Gil's laptop",
  "pairingRequestId": "sunshine-pending-request-id"
}
```

The coordinator verifies lease ownership and submits the PIN only to the
assigned VM. Sunshine credentials never appear in this response.

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
  "streamAddress": "vm1.gilstreaming.internal",
  "streamPort": 47989,
  "sunshineAdminUrl": "https://vm1.gilstreaming.internal:47990",
  "enabled": true
}
```

Sunshine credentials are secret references, not fields returned by an API.
Health, lease owner, lease expiry, and cleanup state are coordinator-owned.
