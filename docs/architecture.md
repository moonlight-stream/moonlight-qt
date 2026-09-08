# GilStreaming architecture

## Deployment boundary

The first deployment has one physical Windows host, GPU-P enabled, and two
Windows guest VMs. Each guest runs Sunshine and can serve one interactive user
at a time. The coordinator is the authority for VM assignment.

```text
GilStreaming client ──HTTPS──> Coordinator
        │                            │
        │ GameStream                 ├── health/pairing ──> Sunshine VM 1
        └────────────────────────────└── health/pairing ──> Sunshine VM 2
```

The streaming path goes directly from the client to its assigned VM. The
coordinator handles authentication and control only; proxying video through it
would add latency and bandwidth cost.

## Session state machine

The coordinator changes a VM through these states:

```text
offline -> available -> reserved -> in_use -> cleaning -> available
                     \-> expired -----------/
```

- `offline`: health checks cannot reach Sunshine or the VM agent.
- `available`: healthy and eligible for assignment.
- `reserved`: an atomic lease exists, but streaming has not started yet.
- `in_use`: the client is renewing its lease while connected.
- `cleaning`: the previous user's processes and profile data are being reset.
- `expired`: a reservation or heartbeat timed out and must be reclaimed.

SQLite is sufficient for a two-VM private deployment. Assignment must occur in
one transaction (`BEGIN IMMEDIATE`) so two requests cannot reserve the same VM.

## Client behavior

The finished GilStreaming client will:

1. Show sign-in/connection status instead of Moonlight's host browser.
2. Request a lease and display `Waiting for a VM` when both slots are occupied.
3. Accept a host only from a valid coordinator response.
4. Disable mDNS discovery, manual host entry, and arbitrary CLI host addresses.
5. Pair with the assigned Sunshine instance through the coordinator.
6. Renew the lease in the background and release it on normal exit.
7. Stop starting new streams if the lease expires or is revoked.

The client is not the security boundary: a user can modify an open-source
binary. Firewall rules, coordinator authorization, and Sunshine client access
control must prevent bypassing the pool.

## Pairing

Each installation of GilStreaming should keep its own Moonlight client identity
and certificate. On first assignment to a VM, the client starts normal pairing
and sends the generated PIN plus lease ID to the coordinator. The coordinator
submits it to the assigned VM's authenticated Sunshine REST API. Sunshine admin
credentials stay on the coordinator and are never returned to the client.

Use a current Sunshine release. Pairing approval must identify the exact pending
pair request when supported; do not rely on a shared constant PIN. A client may
need to pair once with each VM unless VM cleanup preserves Sunshine's approved
client records.

## Network and security

For a private first deployment, place clients, coordinator, and both VM
GameStream endpoints on a private overlay network such as WireGuard or Tailscale.
Expose only the coordinator's HTTPS endpoint publicly if an overlay is not
possible. Never expose Sunshine's admin UI to the public Internet.

- Authenticate every lease request.
- Store only hashed access tokens in the coordinator database.
- Encrypt coordinator traffic with TLS.
- Keep Sunshine admin credentials in coordinator secrets.
- Restrict each VM's Sunshine and admin ports to the overlay/coordinator ACLs.
- Rate-limit sign-in, lease, and pairing endpoints.
- Record lease audit events without logging tokens, PINs, or credentials.
- Revoke the Sunshine client after a lease if users are not fully trusted.

## VM lifecycle

For the initial private deployment, leave both VMs booted and use health checks
to decide availability. On release, terminate launched games and reset the user
session before returning a VM to the pool. Snapshot rollback can be added later,
but it increases turnaround time and can invalidate pairing state.

GPU-P configuration, VM startup, networking, and Sunshine process supervision
belong outside the desktop client. A lightweight agent on each VM can report
health and perform cleanup, while the physical host owns Hyper-V operations.
