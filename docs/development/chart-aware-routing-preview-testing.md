# Chart-aware routing developer-preview testing

This package is an isolated developer preview, not a navigation release. Its
purpose is to exercise the complete chart-aware weather-routing path without
modifying a normal OpenCPN installation or profile.

## Included path

- the hardened OpenCPN core chart-safety API;
- the latest pinned xWeatherRouting solver and chart/depth constraints;
- the modified o-charts batched semantic provider;
- xGRIB, the upgraded Climatology dataset and the companion Polar plug-in.

Licensed charts, chart entitlements, boat polars and forecast files are not
included. Install only charts licensed to the test machine. The o-charts
provider returns derived land/depth classifications to the core; it does not
export plaintext charts.

## Suggested test sequence

1. Install and launch the bundle as described in `README.md`.
2. Add a small, known chart set to the isolated profile. Start with CM93 or
   another ordinary chart source, then repeat with legitimately licensed
   o-charts if available.
3. Load a GRIB which covers the complete route area and time window. Select a
   known boat/polar and record the exact departure time and routing settings.
4. In xWeatherRouting, enable chart-aware routing and set a realistic minimum
   charted depth and safety margin. The feature is selected by default for a
   new profile but remains user-selectable.
5. Run several departure times across a route with known tidal or weather
   sensitivity. Verify both the computed tracks and the result's compact
   weather-source indication (`GRIB` or `GRIB+Clim`). Orange route wind barbs
   mark time steps whose wind came from Climatology rather than the GRIB.
6. Repeat the same calculation without changing charts. The persistent
   semantic cache should make the prepared route footprint much faster.

The optional full semantic atlas is separate idle work. Active routing pauses
atlas generation. Route startup prepares only the bounded scout/direct
footprint, and the unrestricted solver requests additional fail-closed safety
tiles on demand whenever it explores outside that footprint. This avoids
turning a pre-warm estimate into a solver corridor or excluding viable routes.

If no semantic chart source is usable, enforced chart-aware routing fails
closed instead of treating GSHHS as authoritative. Explicitly disable
chart-aware routing to use the ordinary GSHHS land checking for comparison or
manual review; that mode cannot claim the additional chart/depth authority.

## Qualified reference run

The pinned xWeatherRouting runtime revision `30f7a34` was exercised in the
isolated Linux preview on 2026-08-30 using CM93, a 3 m minimum depth and a
0.4 NM land margin. All 13 departure-time candidates completed and all 13
passed final-route chart-safety validation. The warm prepared footprint reused
563 normal/search and 369 endpoint base tiles in 477 ms. Searches remained
broad (roughly 367,000 to 567,000 generated states per completed candidate)
and performed about 2.1 to 2.6 million chart checks per candidate. Revision
`a1340db` is runtime-identical and adds only the omitted Windows test-link
source needed to reproduce the test suite on that platform.

This reference is regression evidence, not proof that a computed route is safe
to navigate. Testers must inspect the resulting track against authoritative
charts and normal passage-planning information.

## Reporting

Please include the package platform, operating system, OpenCPN log, chart
provider, chart-set/update state, cache state (cold or warm), route settings,
boat/polar identity and GRIB coverage. Redact local paths where necessary.
Never publish API tokens, o-charts credentials, entitlement files, licensed
chart data or derived semantic-cache files.

The complete bundle is currently qualified only on Debian 12 x86_64 and
ARM64. Individual xWeatherRouting builds on other platforms do not constitute
qualification of the hardened core plus o-charts integration on those
platforms.

## 31 August responsiveness refresh

The 2026-08-31 candidate retains the qualified solver and bounded route
pre-warm above, and adds the following narrowly scoped fixes:

- idle semantic-atlas extraction yields promptly to GUI activity and active
  weather routing, adapts its batch size to measured latency and avoids
  repeated CM93/chart selection at o-chart boundaries;
- stable provider semantics identify reusable atlas data independently of a
  plug-in binary's path, timestamp or packaging, while an incompatible cache
  is preserved for diagnosis instead of overwritten;
- xGRIB 0.2.4.1 is built from preview revision
  `446814bafe6edfeaf23fadc3f478864f56d81362`, combining release candidate
  `cc61eb752adf92b207933ef06469835b17ac44ea` with the qualified
  external-control environmental provider. It embeds generator 0.1.7 at
  `6156c997191ffbfa9fba39385e84fa06a2fed353`, including isolated workspaces
  for concurrent weather, wave and current jobs.

The candidate workflow builds xGRIB, xWeatherRouting and the modified
o-charts provider from their exact pinned source revisions on each target
architecture. The previous 2026-08-30 package remains available as the
rollback reference.
