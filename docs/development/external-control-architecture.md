# External control architecture decision record

Status: developer candidate, 2026-08-20

## Decision

OpenCPN exposes external control through a versioned `/api/v2` JSON contract.
The HTTP adapter calls small application-service interfaces using ordinary C++
values.  Public service headers contain no HTTP, JSON, wxWidgets, canvas, or
plugin ABI types.  Existing `/api/*`, wxIPC, D-Bus, and native plugin interfaces
remain compatible.

The first vertical slice is deliberately read-only: readiness and capability
discovery, navigation snapshots, route queries, active-route state, and chart
safety queries.  Chart safety has `pass`, `fail`, and `unknown` results; an
unavailable chart engine or unresolved depth is never reported as safe.

Network work is received on the Mongoose I/O thread and represented by a
per-request context.  Application services run on the wx owner thread.  A
bounded wait returns only to the originating request.  Transport objects do not
cross the service boundary.

## Threat model and policy

The protected assets are navigation state, route persistence, outgoing marine
data, chart provenance, and credentials.  Relevant threats include an
untrusted LAN client, leaked credentials, replayed commands, malformed or
oversized requests, slow clients, response cross-talk, and shutdown races.

Consequently, external control is disabled by default and loopback-only when
enabled.  API v2 accepts bearer credentials only in the `Authorization` header,
stores configured tokens as SHA-256 digests, enforces scopes and body limits,
and returns stable machine-readable errors.  LAN access requires an explicit
setting.  Logs must not contain raw bearer tokens or local chart paths.

Read-only scopes in this slice are `navigation:read`, `routes:read`, and
`charts:query`.  Route mutation and activation will use separate
`routes:write` and `routes:activate` scopes, optimistic revisions, idempotency
keys, transactional persistence, and audit records.  Draft creation must never
activate a route.  Direct autopilot, NMEA/N2K output, arbitrary plugin RPC,
filesystem access, and configuration mutation are outside the initial API.

## Compatibility and versioning

The OpenAPI document is the wire contract.  Additive fields and capabilities
may be introduced within v2; incompatible changes require a new API version.
Clients discover runtime capabilities rather than infer them from an OpenCPN
version.  Legacy REST wire formats are unchanged.

## Threading and lifecycle

Queries return immutable snapshots.  All adapters which read GUI-owned state
declare and enforce owner-thread affinity.  Commands will be serialized by the
application owner.  Request deadlines and cancellation are distinct from
service completion.  Shutdown cancels or drains outstanding contexts and must
prevent callbacks after the server or a provider is destroyed.

## Test installation

Development candidates are installed only in the isolated `Test-OpenCPN`
profile.  Its HOME, XDG directories, configuration, plugins, charts, route
database, and port are separate from the sailing installation.  The current
known-good image is retained as a rollback candidate until end-to-end checks
pass.
