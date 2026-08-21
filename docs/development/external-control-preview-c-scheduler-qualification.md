# External-control Preview C Scheduler qualification

Date: 2026-08-21

## Candidate identity

- OpenCPN code revision: `4a783e2f0` plus documentation-only release commit,
  branch `external-control/preview-c-scheduler`;
- xGRIB revision: `edb2431`, branch
  `external-control/scheduler-preview`;
- xWeatherRouting revision: `5dc0460`, branch
  `external-control/scheduler-preview`;
- OpenCPN Scheduler revision: `cbf68db`, branch `main`;
- OpenCPN executable build ID: `66e80e500634b70f22863eace9993a12a84a3e29`;
- xGRIB build ID: `7f788a549b921b4be238225c84baef440bd935c8`;
- xWeatherRouting build ID:
  `ddf57347169f2f97235de3ec4f292548a7eb471f`.

The release-package binaries have the same build IDs as the unstripped
qualification binaries.  Their SHA-256 values differ because CPack strips
debug sections.  The core self-extracting installer was unpacked into a clean
unprivileged root and completed successfully.  The resource-complete xGRIB
and xWeatherRouting archives were also extracted and inspected.

The live candidate used `/tmp/opencpn-preview-c-runtime`, a private network
namespace, an isolated configuration and bearer token, and read-only access
to the test chart collection.  It did not modify or stop the desktop OpenCPN
installation.

## Automated evidence

- OpenCPN: 141/141 eligible registrations passed: 123 labelled deterministic
  and 18 integration tests.  The four desktop-session-dependent
  `IpcClient.*`/`IpcServer.*` registrations are excluded and are not counted
  as passes.
- External API focused suite: 19/19 passed, including typed discovery,
  environmental publication/activation, exact-dataset leases, cancellation,
  bounded events, scoped drafts and fractional-second UTC input.
- xGRIB: 24/24 passed, including reader/merge/XTD/concurrency integration and
  the wxJSON environmental-provider wire contract.
- xWeatherRouting stock-host suite: 176/176 passed at the qualified revision.
- OpenCPN Scheduler: 7/7 passed.
- Python SDK: 4/4 passed.
- least-privilege MCP server: 7/7 passed.
- OpenAPI 3.1 parsed as 22 paths and 24 component schemas; the schedule JSON
  Schema parsed as draft 2020-12.
- A clean AddressSanitizer Preview C core run passed its 123-test deterministic
  subset before the final timestamp-parser patch.  That small parser patch is
  directly covered by the final normal and focused API suites; it has not been
  separately relinked under ASan.  LeakSanitizer remains unavailable under the
  managed ptrace environment.

## Live end-to-end evidence

The installed core loaded xGRIB and xWeatherRouting through the ordinary
native plug-in loader and discovered:

- `environmental-data.xgrib.v1`;
- `route-planning.chart-weather.v1`; and
- the core `route-planning.chart-direct.v1` provider.

Discovery returned typed fields, enumerated forecast resources and installed
boat identities including `Boat.xml`.  The Scheduler stored both example
definitions, then ran the combined environmental-and-route workflow.

xGRIB published dataset `xgrib-919eba2299cefd3848b1` with SHA-256
`919eba2299cefd3848b1fcc9136f5aad51217660e3eabb59d261aca51b92bf9b`,
40,111,022 bytes, coverage 50.5 N/8.5 W to 56.5 N/2.5 W, and fields containing
wind, pressure, waves and currents.  A separate activation response marked
that same identity and checksum active, and the running xGRIB UI opened the
new dataset.

xWeatherRouting received the exact identity, used `Boat.xml`, and returned a
complete four-waypoint route.  Its provenance contains
`weather:xgrib-919eba2299cefd3848b1`.  OpenCPN independently returned
`authority=authoritative`, `decision=pass` with a 5.0 m minimum-depth
constraint and chart database identity
`ocpn-chartdb-v3-50faace519b029a5`.

The Scheduler created one owned draft and did not request activation.  After
a graceful OpenCPN stop and restart, both schedule definitions, the completed
history row, the owned draft GUID and all three provider capabilities were
present.  Both plug-ins unregistered their providers during each clean
shutdown.

## Defects found and corrected during qualification

1. wxJSON on a Unicode host selected its `bool` overload for narrow string
   literals and, where the 64-bit overload was unavailable, for `long long`.
   xGRIB therefore emitted booleans for operation/provenance fields and epoch
   values.  Wire assignments now use explicit `wxString` and unambiguous
   numeric values, with a regression contract.
2. Python emitted a valid ISO 8601 timestamp with fractional seconds, while
   the core accepted only whole seconds despite the OpenAPI `date-time`
   schema.  The Scheduler now emits stable whole-second UTC and the core also
   accepts fractional UTC clients; both behaviours are tested.
3. The evidence recorder initially retained only the pre-activation job result
   (`active=false`).  Qualification was rerun with the separate successful
   activation receipt recorded explicitly, preserving the contract distinction
   between immutable publication and display activation.

## Qualification boundary and limitations

The acquisition workflow used a test-only generator override which copied an
existing valid 40 MB environmental GRIB instead of contacting forecast
providers.  This made the run deterministic and avoided publishing service
credentials or depending on network availability.  The override is not in
any release artifact.  The actual packaged `environmental-grib` helper was
separately executed: capability discovery succeeded and it parsed the same
real GRIB's 387 messages, geographic coverage, 2026-08-20 to 2026-08-22
validity, weather, wave and current components.  Live provider download and
credential failure injection remain separate acceptance work.

This is a Linux x86_64 developer preview, not a production navigation release.
Windows, macOS and Android packaging/behaviour are not qualified.  The server
uses loopback-only development TLS, and the recommended Scheduler credential
lacks `routes:activate`.  Preview C deliberately does not automate plug-in
dialogs, expose provider-private paths, activate routes, or send autopilot
output.

The native plug-in API remains 1.21.  Optional provider registration is
resolved at runtime, so the enhanced xGRIB and xWeatherRouting retain their
ordinary GUI behaviour on older hosts.  Cross-platform stock-host and unload
qualification should precede any proposal to treat these optional entry
points as a stable public plug-in contract.

