# OpenCPN 5.x core-hardening build

This repository contains a conservative, issue-led hardening line based on
OpenCPN upstream commit `e87d2234509636d8e534f0278fb1c6ad8463eb2e`
(5.15.0 development version on 18 August 2026). The unmodified baseline remains
on `master`. The core-only developer build is `hardening/5x-integration`. The
`external-control/preview-b.1-hardening` branch includes this complete
hardening series plus the authenticated external-control API and extension
service/provider work. Its release supplies the exact tested xGRIB and
xWeatherRouting packages without vendoring either plug-in into the core tree.

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
| WMM plug-in messaging | Celestial Navigation crash report | Numeric magnetic-declination JSON is accepted directly, legacy string values remain compatible, and malformed or non-finite values are ignored instead of throwing through the GUI event handler. | Three `PluginComm` parser tests plus an isolated Celestial Navigation/WMM GUI acceptance gate. |
| Chart-aware weather routing | local 5.15 integration | Optional, size-versioned native symbols expose authoritative CM93/S57 land, drying and depth evidence as immutable values. Conservative independent probes recover demonstrable CM93 tile-boundary omissions without treating unknown water as safe; cache identity v3 prevents reuse of older classifications. The existing plug-in ABI remains intact and routing-specific caches stay in the plug-in. | Core-only qualification passed 87/87 tests and the latest xWeatherRouting Preview B tip passes 176/176. Built-in real-chart diagnostics have zero failures. A real xGRIB/polar Holyhead-to-Foyle run at 5 m minimum depth completed six of seven optimized departure candidates and every completed route passed final chart-safety validation. |

## Combined external-control Preview B.1

Preview B.1 is the recommended starting point for developers who need to test
the hardening, external control and chart/weather planning together. It is a
real descendant of Preview B with the issue-led hardening commits merged; it
is not a relabelled Preview B binary. In addition to the table above it offers:

- authenticated, scoped `/api/v2` navigation, route and chart queries;
- transactional draft route operations and restart persistence;
- bounded semantic WebSocket events;
- cancellable asynchronous planning jobs;
- fail-closed chart-direct and resident xWeatherRouting providers;
- Python SDK, `opencpnctl` and least-privilege MCP packages; and
- release assets for the latest tested xGRIB `main` and xWeatherRouting
  `external-control/preview-b` revisions.

The native plug-in API remains version 1.21. The provider registration surface
is optional and versioned separately, so the same xWeatherRouting source still
builds and runs on stock OpenCPN. See the
[Preview B.1 test guide](docs/development/external-control-preview-b-testing.md)
and [combined qualification](docs/development/external-control-preview-b1-qualification.md).

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
cmake --build build-hardening --target opencpn tests buffer_tests -j2
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

The core builds without the optional weather-routing consumer. To reproduce the
combined chart-aware tester artifact, place the compatible plug-in at the exact
tested commit before configuring the build tree:

```sh
git clone https://github.com/pob220/xweather_routing_pi.git \
  plugins/weather_routing_pi
git -C plugins/weather_routing_pi checkout \
  b411e62ed8059549925b2ec66f986cb1ca586db9
```

The core repository does not vendor xWeatherRouting or xGRIB. It retains the
standard bundled `grib_pi`; xWeatherRouting is a separately built, compatible
consumer of the optional chart-safety symbols. The historical v3 convenience
bundle contains xWeatherRouting but not xGRIB. The current external-control
Preview B.1 release contains the full hardening series, xGRIB and
xWeatherRouting; use that when testing the combined system. The core-only
branch remains available for maintainers reviewing the hardening without the
external-control additions.

Build and install both projects into an isolated prefix. The published Arch
Linux convenience bundle is a Debug build so wxWidgets assertions and debug
symbols remain available to testers; it is not the CPack Release archive:

```sh
cmake --install build-hardening --prefix /tmp/opencpn-hardening-install
cmake --install build-weather-routing \
  --prefix /tmp/opencpn-hardening-install
mkdir -p /tmp/opencpn-hardening-install/extra-plugins
cp /tmp/opencpn-hardening-install/lib/opencpn/libxweather_routing_pi.so \
  /tmp/opencpn-hardening-install/extra-plugins/
cp tools/run-hardening-bundle.sh \
  /tmp/opencpn-hardening-install/run-opencpn-hardening.sh
```

Publish the archive together with the commit manifest and test log. The package
is a host-dependent developer artifact, not a signed official release.

The convenience bundle was compiled for the exact `/tmp` prefix. Extract it
there and use its launcher; do not use `-p`, install it over a working OpenCPN,
or point it at the only copy of a vessel profile:

```sh
tar -C /tmp -xzf \
  OpenCPN-5.15.0-core-hardening-v3-chart-aware-debug-arch-x86_64.tar.gz
/tmp/opencpn-hardening-install/run-opencpn-hardening.sh
```

The launcher defaults to a new profile inside the temporary bundle. To test a
copy of an existing profile, set `OCPN_HARDENING_CONFIG_DIR` to that copied
directory. The source build above is the reproducible and portable deliverable;
the binary archive is only a convenience for matching Arch Linux hosts.

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
11. With WMM and Celestial Navigation enabled, open the Celestial Navigation
    dialog and save a sight while WMM publishes numeric declination JSON; verify
    there is no assertion, exception or process exit. Repeat with a legacy
    string-valued message if an older WMM build is supported.
12. Repeat the chart-aware weather-routing acceptance on each target platform
    with a copied profile, representative GRIB/polar and explicit minimum
    depth. The Linux reference scenario is now automated and accepted; compare
    additional proposed routes to current official charts before field use.

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
