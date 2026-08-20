# External-control Preview B qualification

Date: 2026-08-20

## Candidate identity

- OpenCPN branch: `external-control/preview-b` (publication name)
- OpenCPN commit: `b2a6b521c` (`Keep planning cancellation responsive during chart work`)
- xWeatherRouting branch: `external-control/preview-b` (publication name)
- xWeatherRouting commit: `8131ff4` (`Clear resident state after startup cancellation`)
- Isolated installation: `/home/paul/Test-OpenCPN/candidates/worktree`
- Installed OpenCPN SHA-256:
  `f7e413c86324e2387fb35993154be7248326d1c8c4bb35ff57a6a10f3b1a96d3`
- Installed xWeatherRouting SHA-256:
  `34bf24b8036fd95b3f8a8f9f1f0ae5c4d4c1a81e29a04cac4840865bde3d1ffa`
- Qualified stock-host xWeatherRouting SHA-256:
  `a475bc6410c494e38114dd053d88b73e907dc3c13d49c6de5926b80b85188d4e`

The previous candidate binaries remain beside the installed files as rollback
copies. Test-OpenCPN has isolated configuration, charts, plug-ins, token,
database and TCP listener. The day-to-day desktop OpenCPN core and xGRIB were
not modified by this qualification.

The published Linux core installer was extracted successfully into an
unprivileged isolated directory and contained 791 payload files. The published
resource-complete core and xWeatherRouting package executables have the same
GNU build IDs as the unstripped binaries used for qualification; packaging
only strips debug data. Both xWeatherRouting packages contain 853 entries,
including `Boat.xml`, example polars, plug-in data and locales. The xGRIB
package contains 23,690 entries, including its private helper runtime and
plug-in data. Release checksums cover the complete packages as well as the raw
diagnostic artifacts.

## Automated evidence

- OpenCPN deterministic eligible CTest suite: 113/113 passed sequentially.
- xWeatherRouting stock-host suite: 176/176 passed.
- xGRIB suite: 22/22 passed.
- Python SDK suite: 3/3 passed.
- MCP deterministic protocol suite: 7/7 passed.
- OpenAPI 3.1 parse: 16 paths and 17 component schemas.
- Clean GCC AddressSanitizer build: 28/28 external-control, planning-host and
  chart-safety tests passed with abort-on-error enabled. LeakSanitizer was
  disabled because it cannot run under the managed ptrace environment.
- Both stock-host and hardened-host xWeatherRouting shared libraries built
  successfully. The native plug-in API remains 1.21.

The historical aggregate test wrapper and four session-dependent IPC tests are
not counted as passes. Three `IpcClient.*` registrations block waiting for a
desktop-session peer in this environment and `IpcServer.Commands` is likewise
session-dependent. The 113 eligible registrations are independently runnable;
the suite is run serially because several older fixtures share
`opencpn.conf`.

## Live isolated evidence

The installed candidate passed the following against its real HTTPS listener,
real plug-in loader and CM93 chart database:

- authenticated version, readiness and runtime capability discovery;
- deterministic service-before-plug-in startup: xWeatherRouting registers
  `route-planning.chart-weather.v1` during normal plug-in loading;
- xGRIB and xWeatherRouting load in place of the bundled GRIB and original
  Weather Routing plug-ins;
- a resident xWeatherRouting request completed with exact start/destination
  geometry, active weather/current provenance and an independent OpenCPN
  chart-safety result of `authoritative/pass` at 5.0 m minimum depth and
  0.1 NM land margin;
- a land-crossing request failed closed rather than returning a draft;
- a 240-hour, seven-departure request accepted cancellation while initial
  chart work was active and reached terminal `cancelled` in about 14 seconds;
- the same resident provider immediately accepted and completed a second
  authoritative 5 m request, proving cancellation left no `provider_busy`
  session or abandoned calculation; and
- route outputs remain drafts and are never automatically activated.

Earlier Preview A qualification also covered scoped WebSocket events, CLI and
MCP live workflows, transactional draft persistence across restart and guarded
route commands. Those contracts are unchanged by Preview B.

## Defects found and corrected during qualification

1. Plug-ins loaded before the extension-service registry existed, so the
   provider could not register on a cold start. Service construction now
   precedes plug-in loading; network admission still starts later.
2. Planning cancellation was dispatched through the wx thread and timed out
   behind synchronous chart prewarm. Only planning status/result/cancel
   endpoints now use the thread-safe transport path.
3. xWeatherRouting held its start-state mutex across the entire owner-thread
   startup and could not observe cancellation. External prewarm now runs in
   bounded batches with a worker-visible cancellation flag.
4. Early startup cancellation stopped computation but retained the resident
   headless-session marker. Cleanup now precedes the terminal job state, so the
   provider is reusable.
5. Several legacy unit registrations relied on ordering or transferred
   ownership of a static wx logger. The test fixtures now own their state
   independently; production behaviour is unchanged.

## Known limitations before a general prerelease

- This is a Linux developer preview, not a navigation release. Windows and
  macOS builds, and an explicit Android capability decision, remain open.
- The API accepts an installed polar identity but cannot yet enumerate valid
  xWeatherRouting polar identities. Invalid names fail explicitly with
  `polar_not_found`.
- Weather/current selection is `active` only. There is no dataset catalogue or
  upload/path API.
- The short authoritative route is a deterministic integration canary. The
  difficult Holyhead-to-Foyle request was used to exercise cancellation, not
  claimed as a completed weather-route qualification with the tiny active test
  GRIB.
- Focused UBSan and TSan qualification and cross-platform plug-in unload tests
  remain outstanding.
- No signed cross-platform installer or package-index upload has been made.

These limitations are capability-visible. Unknown, fallback or incomplete
chart/depth evidence cannot become a passing route, provider geometry is
treated as untrusted input and independently revalidated, and completed plans
remain drafts until a separately scoped command is issued.
