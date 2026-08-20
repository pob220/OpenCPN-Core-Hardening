# External-control developer candidate qualification

Date: 2026-08-20

## Candidate identity

- OpenCPN branch: `external-control/integration`
- OpenCPN implementation commit: `e2678db9f` (`Add semantic events and cancellable planning jobs`)
- OpenCPN qualification/documentation commit: `7a7ea6638`
- xWeatherRouting branch: `xweather-routing-alpha`
- xWeatherRouting commit: `fb10a97` (`Expose route geometry in headless planning results`)
- Isolated installation: `/home/paul/Test-OpenCPN/candidates/worktree`
- Candidate OpenCPN SHA-256: `dd024de2d8f6273d554f6c1e23a9af370d5e20415fd7370727487591e2949d0f`
- Stock OpenCPN SHA-256 (unchanged): `6d0518602f1540f5f2189d92dc4c41004f8cf3a59757a893eb6df5529d5552d9`

The previous candidate binary and plugin were retained beside the installed
files as rollback copies. The Test-OpenCPN HOME, XDG directories, profile,
charts, plugins, token, database, and TCP port are isolated from the sailing
installation.

## Automated evidence

- OpenCPN CTest suite: 100/100 passed.
- External API/event/planning focused suite: 17/17 passed.
- xWeatherRouting suite: 172/172 passed.
- Python SDK suite: 3/3 passed.
- MCP deterministic protocol suite: 7/7 passed.
- OpenAPI 3.1 parse: 16 paths and 17 component schemas.
- Python SDK and MCP source archives and wheels built successfully.
- Core and plugin builds completed without errors.
- Clean GCC AddressSanitizer build: 26/26 deterministic external-control,
  planning-host and chart-safety tests passed with abort-on-error enabled.
  LeakSanitizer was disabled because the managed test environment runs under
  ptrace, which LeakSanitizer explicitly does not support.

## Live isolated evidence

The installed candidate passed all of the following against its real HTTPS
listener and chart database:

- authenticated version, readiness, and runtime capability discovery;
- scoped WebSocket initial snapshot and subscription acknowledgement;
- planning-job semantic events with monotonic sequence numbers;
- authoritative chart validation of known-clear Irish Sea segments;
- a completed `route-planning.chart-direct.v1` job returning a two-point draft,
  chart identity, constraints, provenance, and final authoritative safety pass;
- fail-closed rejection of a land-crossing planning request;
- `opencpnctl status` against the installed server;
- MCP 2026-07-28 discovery, tool listing, status, plan submission, and completed
  plan-result retrieval without an LLM;
- transactional draft creation, process restart, route retrieval with the same
  revision and geometry, and transactional cleanup.

## Known limitations before a general prerelease

- The generic bounded planning host and chart-direct provider are live. The
  xWeatherRouting headless contract now exports complete route geometry, but a
  resident plugin-to-core planning provider adapter is not yet registered.
  Consequently, chart/weather optimization is not advertised as a runtime API
  capability in this candidate.
- This qualification is Linux-only. Windows and macOS builds, and an explicit
  Android capability decision, remain outstanding.
- Focused UBSan and TSan qualification, beyond the clean ASan gate above, has
  not yet been run for the WebSocket and job-host code.
- SDK WebSocket reconnection policy and cross-version server contract matrices
  need broader qualification.
- Binary and Python packages are developer artifacts only; no package-index
  upload or platform-signed installer publication has been performed.

The historical aggregate CTest registration is not itself a release gate: it
contains serial/CAN, user-session IPC and GUI fixtures which are unavailable or
stateful on some hosts. A mandatory deterministic CI gate now covers the new
external-control, planning-host and chart-safety slice while those fixtures are
separated. This qualification does not silently count fixture absence as a
pass.

These limitations are capability-visible and do not weaken the conservative
chart-safety rule: fallback or unknown chart evidence cannot complete a plan as
safe, and completed plans remain drafts until a separate authorized command.
