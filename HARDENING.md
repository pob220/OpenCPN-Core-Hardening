# OpenCPN 5.x core-hardening build

This repository contains a conservative, issue-led hardening line based on
OpenCPN upstream commit `e87d2234509636d8e534f0278fb1c6ad8463eb2e`
(5.15.0 development version on 18 August 2026). The unmodified baseline remains
on `master`. The combined developer build is `hardening/5x-integration`.

This is not an official OpenCPN release. Do not use it as the sole means of
navigation. Back up configuration and navigation data before field testing.

## What is included

| Area | Issue(s) | Result | Evidence in this repository |
|---|---|---|---|
| Test gate | — | Deterministic CTest cases are discoverable and blocking in Linux CI, including the sanitizer configuration. Host-dependent tests are labelled `integration`. | `ctest -L deterministic`; `test/README.md` |
| N2K waypoint closing velocity | #5311 | PGN 129284 now encodes knots as metres/second at the wire boundary. | Payload is decoded back in `AutopilotOutput.Pgn129284EncodesClosingVelocityInMetersPerSecond`. |
| Signal K AIS | #5145 | Malformed/wrong-typed delta members are rejected before state mutation. | `AisSignalK.MalformedDeltaIsIgnored`. |
| First-run initialization | #5287 | Startup no longer waits forever for a chart rebuild which was not started; settings commands are gated until deferred initialization completes. | Full application build; manual first-run checklist below. A deterministic GUI lifecycle test is still desirable. |
| Route/routepoint persistence | #5200, #5202 | Route deletion uses an explicit SQLite transaction and orphan policy, preserving shared/intentional points and rolling back on failure. | Two `NavobjRouteDeleteTest` cases, including forced rollback. |
| SENC lifetime | #3851 | SENC shutdown waits for borrowed worker state before deleting tickets. | `AsyncWorkerLifecycle` tests and full application build. Licensed-chart stress testing remains required before claiming every historical chart crash closed. |
| Actisense input | #4554/#3316/#4686 cluster | Framing is bounded, validates length/checksum, and resynchronizes after malformed input. | Four `ActisenseFramer` byte-stream tests. |
| N2K teardown | #4554 cluster | The serial worker is joinable and `Close()` is idempotent; owner destruction cannot abandon a detached raw callback. | `N2kSerial.MissingDeviceJoinsWorkerOnClose`. Hardware validation remains required. |
| Route transition output | #4103 | All leg fields are recomputed as one pure value before transition output, including XTE direction and antimeridian bearing. | Three `RouteLegState` tests. Real autopilot families must validate sentence semantics before issue closure. |
| Raster texture jobs | #4842, #4983 | Jobs retain shared factory ownership, cancellation is atomic, result buffers are RAII-owned, and destruction drains workers/events. | Full OpenGL build plus lifecycle test seam. macOS cache-pressure testing remains required. |
| Plug-in chart catalogue | #5170 | Add/remove no longer deletes and replaces global `ChartData` while asynchronous users can retain it; mutation drains dependent work and reindexes the existing object. | Full OpenGL build. A plug-in-driven catalogue stress test remains required. |
| AIS safety broadcasts | #5069 | Valid type-14 messages are retained independently of position, published as a typed event, and displayed as a warning for any MMSI without activating CPA/SART audio policy. | `AIS.StandaloneType14PublishesSafetyMessage`. |
| Chart-aware weather routing | local 5.15 integration | Optional, size-versioned native symbols expose authoritative CM93/S57 land, drying and depth evidence as immutable values. The existing plug-in ABI remains intact and routing-specific caches stay in the plug-in. | Real CM93 diagnostic: 91 masks, 146 base tiles, 245,426 classified cells and zero failures; compatible plug-in: 158/158 tests and a successful live host/cache handshake. |

The complete rationale, evidence levels, counterarguments and backlog taxonomy
are in [the architecture audit](docs/development/open-issue-core-architecture-audit.md).
The implementation sequence is in
[the hardening plan](docs/development/core-hardening-implementation-plan.md).
Exact commits and branch names are in
[the build manifest](docs/development/hardening-build-manifest.md).
The chart-service contract, provenance, limits and reproduction evidence are in
[the chart-aware routing integration note](docs/development/chart-aware-routing-integration.md).

## Build and deterministic tests

On a Linux host with the normal OpenCPN build dependencies:

```sh
git clone --recursive https://github.com/pob220/OpenCPN-Core-Hardening.git
cd OpenCPN-Core-Hardening
git switch hardening/5x-integration
cmake -S . -B build-hardening \
  -DCMAKE_BUILD_TYPE=Debug \
  -DOCPN_BUILD_TEST=ON \
  -DCMAKE_INSTALL_PREFIX="$PWD/install-hardening"
cmake --build build-hardening --target opencpn tests -j2
ctest --test-dir build-hardening --output-on-failure -L deterministic
cmake --install build-hardening
```

Run the installed executable from an isolated profile. The exact command-line
profile options differ by platform; never point a first test at the only copy
of live vessel data.

Environment-dependent tests are deliberately separate:

```sh
ctest --test-dir build-hardening --output-on-failure -L integration
```

These require services/devices such as D-Bus, SocketCAN, loopback networking or
the IPC server. Their failure on an unprepared workstation does not invalidate
the deterministic suite, but must not be reported as a product regression
without reading the test prerequisite.

## Sanitizer check

```sh
cmake -S . -B build-asan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DOCPN_BUILD_TEST=ON \
  -DOCPN_USE_GL=OFF \
  -DENABLE_SANITIZER=address
cmake --build build-asan --target tests buffer_tests -j2
ctest --test-dir build-asan --output-on-failure -L deterministic
```

Use UBSan and focused TSan runs where supported. Linux sanitizers do not close
macOS OpenGL, Windows driver or hardware-specific issues by themselves.

## Create a Linux tester package

After a successful complete build:

```sh
cd build-hardening
cpack -G TGZ
```

Publish the archive together with the commit manifest and test log. The package
is a developer artifact, not a signed official release.

## Manual acceptance checklist

Use a copied profile and synthetic/test inputs.

1. First run with an empty chart directory list; wait for initialization, then
   open Settings from both menu and toolbar.
2. Replay the malformed Signal K fixture and confirm the process remains alive,
   valid AIS fields still update, and invalid fields do not.
3. Delete routes containing route-only, shared, promoted and isolated points;
   restart and verify the same invariants in the route manager and database.
4. Rebuild S57/SENC charts while forcing cache pressure, then close OpenCPN at
   each job phase. Repeat under ASan on every platform which supports it.
5. With an Actisense device, exercise receive, transmit, cable removal, rapid
   connection reconfiguration and shutdown. Capture logs and emitted PGNs.
6. Decode PGN 129284 on a second implementation/device and verify closing
   velocity in m/s for toward, oblique and receding legs.
7. Replay an autopilot route transition in simulation before connecting a live
   pilot. Compare APB/RMB direction and XTE with the baseline and each intended
   autopilot family.
8. On macOS, rebuild/clear raster texture cache repeatedly while panning and
   shutting down.
9. From a test plug-in, add/remove catalogue entries repeatedly during chart
   refresh; verify no stale chart reference and a clean next startup.
10. Replay standalone and target-associated AIS type-14 messages from several
    MMSI classes; verify visible warning text, trimming, acknowledgement, and no
    unexpected audible CPA/SART alarm.
11. Install the compatible chart-aware weather-routing plug-in, use a copied
    profile, representative GRIB/polar and explicit minimum depth, and compare
    the proposed route to the reference build and current official charts.

## Review model

Each production change has an isolated topic branch. Review and upstream these
branches independently; do not submit the cumulative integration branch as one
large pull request. Keep the native plug-in API/ABI compatible. Where a patch
adds a boundary, the boundary exists because the regression requires an
explicit unit, transaction, state or lifetime rule—not to pursue C++ fashion.

Known limitations and unproven closure claims are intentional. In particular,
the SENC, texture, plug-in catalogue, N2K hardware and autopilot changes need
their platform/domain matrices before maintainers should close every clustered
issue.
