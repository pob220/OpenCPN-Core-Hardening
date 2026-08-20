# External-control Preview B

Preview B extends Preview A with a resident, cancellable weather-routing
provider. It is a developer candidate for isolated testing, not yet a
navigation release and not a replacement for a known-good onboard
installation.

## Added vertical slice

- optional plug-in registration of
  `route-planning.chart-weather.v1`;
- xWeatherRouting request translation for departure windows, routing effort,
  active weather/current datasets, installed polar identity, minimum depth,
  land margin, and explicit climatology fallback;
- complete candidate geometry returned by xWeatherRouting;
- independent final chart-safety validation by core before any result becomes
  an external-control draft;
- provider discovery removal, cancellation, job pinning, and unload veto while
  callbacks are still active.

The built-in `route-planning.chart-direct.v1` remains available as the small,
deterministic baseline.

## Compatibility boundary

The existing native plug-in API remains version 1.21. Preview B does not add a
virtual method, change an existing plug-in class, or require a stock plug-in
host to understand the provider mechanism.

Instead, hardened OpenCPN exports a separate versioned C registration surface.
xWeatherRouting resolves it at runtime. On stock OpenCPN the symbols are absent,
registration becomes an informational no-op, and all existing GUI behavior is
retained. This separates the navigation service offered to external clients
from both the HTTP transport and the native plug-in object ABI.

## Safety boundary

Provider geometry is untrusted input even when it originates in an installed
plug-in. Core rejects malformed, non-finite, out-of-range, incomplete, failed,
or empty results. A complete result is validated again using the authoritative
chart service on the wx owner thread. Unknown chart/depth evidence never
becomes a passing draft.

Planning always produces a draft. It cannot activate a route, send autopilot
output, mutate arbitrary settings, access arbitrary files, or install a
plug-in. Those operations remain outside the planning capability.

## Preview plug-ins

The Preview B test installation uses xGRIB and xWeatherRouting in place of the
bundled GRIB and original Weather Routing plug-ins. Polar data is unchanged.
Climatology is optional and uses its existing plug-in message contract; a
request must explicitly enable fallback, and any compatible Climatology build
may satisfy it.

The external planning adapter does not depend on a particular xGRIB binary.
Preview B currently identifies only the weather/current datasets active in the
running plug-ins. Stable dataset discovery and selection are later contract
work.

## Deferred deliberately

- native plug-in ABI changes;
- automatic import or activation of a completed route;
- arbitrary plug-in RPC;
- multiple concurrent jobs inside one xWeatherRouting instance;
- remote paths or uploads for polar/weather data;
- a general plugin-service framework before this narrow boundary has field
  evidence;
- signed cross-platform installers.

The wire contract is [api/openapi-v2.yaml](../../api/openapi-v2.yaml), and the
provider-side lifecycle details are documented in the xWeatherRouting
repository.
