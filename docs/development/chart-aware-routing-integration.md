# Chart-aware weather-routing integration

**Integration date:** 19 August 2026  
**Core branch:** `feature/chart-aware-routing`  
**Core tip:** `89e25705e8f1c7b180a8d7a8c067d629f391bf4d`  
**Compatible plug-in:** `pob220/weather_routing_pi`, commit
`f6891f8a78f49a59582eb320a67445ad498046f3`

This note records what was imported from the locally tested OpenCPN 5.15.0
checkout, what was deliberately not imported, and the evidence behind the
developer build. It is not a claim that chart-aware routing is ready for
unattended navigation. OpenCPN and the plug-in remain decision aids; routes
must be checked against current official charts and conditions.

## Scope and provenance

The desktop launcher was traced to the binary built from the local
`safer-renderer` branch. The chart-safety commit series was then replayed onto
the clean hardening integration branch. No desktop files, installed binaries,
user configuration, chart database or navigation objects were changed.

Only the chart-safety/core-service series was imported. Unrelated experimental
renderer and GRIB-generator work in the working checkout was not inherited.
The weather-routing plug-in was built from the exact commit shown above against
the hardening headers; its source tree is not vendored into this repository.

The series adds:

- chart-backed segment classification for CM93 and S57 sources;
- land, drying and minimum-depth results with explicit unknown/unavailable
  states;
- immutable raw hazard/depth tiles and route-shaped masks;
- chart-database identity and cache invalidation callbacks;
- main-thread request servicing and route-mask pins, so worker threads do not
  borrow mutable chart objects;
- optional plug-in-owned RAM/persistent caches; and
- the `--segment_safety_test` diagnostic runner.

## Service and compatibility boundary

The useful architectural split is based on ownership, not library names:

```text
OpenCPN chart adapters (main thread)
    parse CM93/S57 and select authoritative chart evidence
        -> immutable hazard/depth value tiles
            -> optional native plug-in entry points
                -> weather-routing-owned masks, persistence and graph search
```

OpenCPN remains the authority for chart parsing and object classification. The
plug-in remains the authority for routing-specific derived masks and cache
policy. Worker routing code consumes immutable values rather than OpenCPN chart
pointers.

The new functions are optional native symbols discovered by the plug-in at
runtime. Existing plug-in entry points and ABI are retained. A plug-in running
against an older core detects that the service is unavailable and falls back;
the hardening core does not require existing plug-ins to adopt the service.
All versioned structures include `struct_size` checks so newer fields are not
read through an older caller's layout.

The linked binary exports the seven functions needed by the tested plug-in:

- cache registration;
- chart-database identity query;
- segment safety query;
- raw-tile prewarm;
- hazard-snapshot prewarm;
- pending-request service; and
- route-mask pin release.

This is concrete evidence for separating **what** chart services OpenCPN offers
from **how** the native plug-in host exposes them. It is not yet a fully
headless chart library: the implementation still lives mainly in
`gui/src/ocpn_plugin_gui.cpp` and relies on main-thread chart managers.

## Verification performed

### Core and sanitizers

- The complete Debug `opencpn` target linked with warnings treated as errors.
- 71/71 tests labelled `deterministic` passed after integration.
- 18/18 safety-relevant tests passed in a separate AddressSanitizer build with
  leak detection disabled because the execution sandbox blocks LeakSanitizer's
  ptrace mechanism. This is not an LSan result.
- Four host-dependent loopback cases fail on this workstation when their
  network handles are unavailable. They are labelled `integration` and are not
  represented as deterministic product regressions.

### Plug-in and host handshake

- The exact weather-routing plug-in commit compiled against the hardening
  headers.
- 158/158 plug-in tests passed. These cover exact cache payloads, persistence,
  identity invalidation, depth completeness, fail-closed missing evidence,
  minimum depth, concurrent immutable masks, prohibited land shortcuts,
  blocked passages, provider hooks and route-search recovery/fallback.
- OpenCPN loaded the resulting shared library in an isolated profile.
- The plug-in registered its tile cache, reported full chart-aware safety, and
  cleanly unregistered the cache during deactivation.
- The built-in segment-safety diagnostic completed with `failures=0` while the
  plug-in was loaded.

### Real chart corpus

An isolated copy of the user's chart/profile configuration exercised the local
CM93 2015 corpus. The user's original profile and chart files were read-only
inputs and were not modified.

The diagnostic requested 91 route masks, built 146 base tiles and classified
245,426 cells. Its second pass reused all 91 route masks and all 146 base tiles.
Land-crossing cases passed, and depth cases included a 250 metre minimum-depth
query which correctly returned `TOO_SHALLOW`. The final result was
`SEGMENT_SAFETY_TEST end failures=0`.

This proves the core can obtain chart evidence, exchange it with the plug-in
cache boundary and enforce the diagnostic land/depth cases. It does not replace
a manual route computation with representative GRIB data, vessel polar and
safety settings.

## Assertion reported during testing

The real-chart diagnostic initially reproduced the wxWidgets dialog:

```text
wxArgNormalizer(): format specifier doesn't match argument type
```

The exact failing log statement formatted `wxStopWatch::Time()` (a `long`) with
`%d`. Commit `89e25705e` changes the `WR_ROUTE_MASK_BUILD` field to `%ld`. The
same diagnostic then completed normally with zero failures. This was a logging
type mismatch, not evidence that the underlying depth classification was
wrong, but in a wxWidgets assertion build it interrupted normal use.

## Deliberate limitations and next review steps

The current patch is a compatibility and field-test integration, not a suitable
single upstream pull request. It changes 12 files and adds about 8,500 lines,
with most implementation in `ocpn_plugin_gui.cpp`. Before upstreaming:

1. land the small chart-adapter classification hooks with focused CM93/S57
   fixtures;
2. land the size-versioned value structures and one read-only query path;
3. land the main-thread request queue plus shutdown/cancellation tests;
4. land one immutable tile/prewarm consumer and measure it;
5. add cache identity/invalidation independently;
6. add the routing-shaped mask optimization only after equivalence tests; and
7. keep plug-in persistence and graph-search logic in the plug-in.

Do not first extract a grand `ocpn-charts` library. The immediate boundary is
valuable because it prevents cross-thread chart-object access and has a real
consumer. Once CM93/S57 classification has fixture coverage and no longer
requires UI globals, that tested code can seed a headless chart-query service.

## Remaining acceptance work

- Run representative end-to-end weather routes with supplied GRIB, polar,
  start/end points and minimum depth, comparing route geometry to the desktop
  reference build.
- Exercise S57/SENC and mixed chart stacks, not only CM93.
- Test chart database rebuild/replacement while requests and cache entries are
  active.
- Validate Windows and macOS builds and native symbol discovery.
- Package the plug-in's toolbar SVG assets with the shared library; a bare build
  directory correctly falls back to its embedded icon but logs an asset warning.
- Stress shutdown, plug-in disable/re-enable and chart-directory changes under
  ASan/TSan where supported.

