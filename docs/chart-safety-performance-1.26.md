# Companion chart-safety performance changes for xWeatherRouting 1.26

The existing host service extracts chart semantics on the application thread and
publishes immutable tiles to routing workers. A prepared CM93 tile has 41 × 41
samples. The old path repeated render eligibility, object-list allocation and
attribute-string decoding for those 1,681 samples, even though the chart working
set and attribute values were unchanged during the tile.

## Changes

* Collect the same rendering-eligible area rule list once per prepared chart
  within the tile. Preserve the boundary style and rendering eligibility checks.
* Decode the existing classifier's land, drying and minimum-depth attributes
  once. Omit area rules which cannot contribute to that classifier. Continue
  to use the original float-coordinate polygon-selection function at every cell.
* Hold borrowed rule references only inside the tile build. Clear all references
  before raw chart fallback and depth-boundary recovery, which can mutate charts.
* Rebuild CM93 line geometry, contour tables and object contexts only after a
  native base cell or subcell was actually loaded. Absent cells are still retried.
* Suppress per-native-cell global busy-spinner operations only inside a nested,
  thread-local semantic-query scope. Chart display retains normal feedback.

No plugin API exports or request/result layouts are changed. The new S57 helper
is nonvirtual and adds no chart-instance data. The xWeatherRouting companion
change shares the existing request-service budget across all waiting routes.

## Safety and remaining costs

The grid, authoritative chart selection, exact polygon test, clearance halo,
conservative boundary-depth recovery and unknown-data policy are unchanged.
An unchanged tile's existing persistent cache remains valid. Qualification
compares class, hazard flags, depth and depth-completeness for common tiles and
requires final passage safety validation; route-choice differences are recorded.

These changes remove redundant host work; they do not move chart access onto
routing threads. A synchronous chart operation can exceed the nominal GUI
service budget. Workers still suspend when a search expands beyond prepared
coverage. Preparation corridors, asynchronous chart loading and a spatially
indexed semantic geometry interface are possible later work, but are not part
of this patch and must preserve chart priority and conservative depth evidence.

## Regression procedure

Build the normal OpenCPN target and core tests. Run core tests with an isolated
REST port if an OpenCPN instance is already listening; keep generated certificates
inside the private test build. Build the plugin against API 1.21 and also run a
normal route on an unmodified stock host.

Use identical weather, polar, engine configuration, chart group and thresholds
for baseline/candidate runs. Run cold and warm measurements sequentially without
competing builds, and compare final safety checks and persistent tile semantics.
Include coastal land, shallow water, clearance, no coverage, route edits,
checker cancellation, concurrent departure candidates and the optional native
route-checking workflow. Do not substitute unknown depth with assumed deep water
or reduce the safety-grid resolution to obtain a better timing.

## Corrected-release comparison

The final shared xWeatherRouting 1.26 source is
`caa9152cb78151129175aeddb98708652c248c68`, including the corrected bounded
endpoint clearance and continuous chart-grid chord traversal. A sequential,
unprofiled comparison on the same laptop with fresh private caches completed
in 346.527 seconds for 1.25 / previous core and 129.414 seconds for corrected
1.26 / companion core, approximately 2.68 times faster. This is one cold run
per version on a fixed Hawaiian substitute workload, using CM93, full GSHHG,
a Lagoon 560 polar, 1.5 m depth and 0.1 NM clearance. It is not a reproduction
of Quinton's original forecast and chart configuration or a general speed
guarantee. The earlier warm-cache comparison showed no measured improvement.

The route choice differs: 205.791 NM / 237,296 seconds of passage time for
1.25, versus 220.951 NM / 275,819 seconds for corrected 1.26. Both reproduce
the exact paths independently checked with the unchanged checker: 835/835
baseline sections and 918/918 corrected sections clear, with no unknown
sections. The cause of that route-choice difference is not established by
the safety audit; unchanged route quality is not claimed. The final corrected
comparison supersedes the earlier pre-correction 2.73-times timing result.
