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

## 9 September Weather Routing 1.17.1 refresh

The 2026-09-09 bundle updates the routing component to the clean Weather
Routing 1.17.1.0 source at revision
`c8f7c2db07f7e413e9692421be1bdee936b4adf3`. It is built with the isolated
`xWeatherRouting` identity used by this developer preview, so it does not
replace a tester's ordinary Weather Routing installation.

This refresh includes the 1.17 routing-resource and weather-table hardening,
the current API 1.21 compatibility work, clearer progress while chart safety
scouts additional tiles, and the 1.17.1 routing-table lifetime fix. The table
now detaches from route overlays before they are deleted and validates that a
route remains managed before dereferencing it. It is built directly from the
pinned `pob220/weather_routing_pi` source revision; no tester-local or
third-party working-tree changes are included.

## 7 October 2026 refresh

The current bundle pins xWeatherRouting 1.26 with its corrected local endpoint
clearance policy. Coastal endpoint access is limited to twice the configured
margin, with a 0.5 NM minimum and 2 NM maximum. An endpoint flag cannot waive the
margin along an arbitrarily long plotted chord. The full margin is enforced
outside that access area; land, exclusion and depth constraints remain active
within it. Test both a local coastal departure and an offshore passage. A
failed route search is not evidence that no safe passage exists.

The core prepares CM93 area rules and attributes once per safety tile, retains
exact per-point polygon classification, and avoids rebuilding geometry after
an empty missing-cell lookup. xWeatherRouting services the shared chart queue
once per timer callback across concurrent routes. These reduce cold chart
preparation work; the earlier benchmark did not measure a warm-cache gain.
S-57/S-63 provider expansion remains separate from this performance refresh.

xGRIB 0.3.7 includes the current date-line and older-ecCodes wave fixes while
retaining the preview's optional environmental-data API provider. Celestial
Navigation 2.9.7 includes the Compact runtime payload, restored desktop FIX,
additional lunar checks, and its desktop guide. Optional eclipse/lunar-terrain
packs remain opt-in. Both native architectures must pass the fresh component,
GUI, import/replacement, isolated-install and API/MCP checks before publication.

The corrected native build passes 416 unit tests. A Holyhead–Conwy GUI batch
completes 20 of 25 hourly departures at 1 NM clearance and 3 m minimum depth;
five search failures remain. The separately audited single passage has full
clearance outside its bounded endpoint areas and land/depth checks throughout.
Do not treat the batch as proof that every feasible departure is found.
Auto retains Quick, Standard and Professional; Alternative is not added to it.

The routing pin is the published 1.26 alpha source, including its Android
packaging qualification; these archives are native Debian 12 desktop builds.
The final controlled cold-cache comparison is approximately 2.68 times faster
on the fixed Hawaiian substitute fixture, while selecting a longer passage.
See `CHART-SAFETY-PERFORMANCE-1.26.md` beside this guide for the method,
independent safety checks and route-quality limitation. This is not a claim
that every chart-aware passage improves by that factor.

## 7 October 2026 second refresh — 1.27 / 0.3.9

Use the **2026-10-07.2** archives for this refresh. The earlier October and
September archives remain available for rollback. Install into a fresh
directory and use its own launcher; it enables
`OCPN_CHART_SAFETY_GEOMETRY_PROOF=1`.

xWeatherRouting 1.27 retains the corrected 1.26 routing engines and coastal
endpoint policy. The updated core copies immutable native safety geometry,
indexes soundings, rejects distant objects by authoritative bounds and reuses
whole-area coverage evidence. Uniform CM93 tile proofs require complete
highest-detail coverage and known conservative area depth. Every isolated
rock, wreck or obstruction prevents this shortcut, including deep known ones;
missing danger depth remains unknown. Tiny reefs, shallows, drying areas,
coverage holes and boundary contacts remain part of detailed checks. Changed
cache identities invalidate earlier derived data. See
`CHART-SAFETY-PERFORMANCE-1.27.md` for measured evidence and limitations.

xGRIB 0.3.9 / Generator 0.3.4 retains the optional preview environmental API
provider, with immediate area/time checks and clearer Copernicus authentication
errors. Celestial Navigation 2.9.7 and the other component pins are retained.
Native plugin API 1.21 remains unchanged. On stock OpenCPN, xWeatherRouting
still uses GSHHG with chart-awareness controls disabled; an explicit chart-depth
requirement is refused rather than silently weakened. These developer bundles
supply the enhanced core. No real licensed S-63 qualification is claimed.

Both Debian 12 architectures must pass the new geometry and isolated-danger
fixtures, component tests, clean-install/import/replacement, GUI and API/MCP
checks before release publication. Earlier benchmark and coastal batch results
above are historical 1.26 evidence, not a fresh 1.27 batch claim.
