# Developer preview refresh — 12 September 2026

This preview branch refreshes xWeatherRouting to 1.17.4.0, xGRIB to 0.2.5.2
and Environmental GRIB Generator to 0.1.8. The existing core, other component
pins and isolated installation model are retained.

Routing preserves destination connections when the next search expansion would
exhaust its budget, records the running version/bitness and improves Windows
memory diagnostics. A longer synthetic route still reaches the bounded search
limit; this refresh does not claim to resolve every reported routing failure.

xGRIB adds live offline size estimates and measured GRIB totals. Generator 0.1.8
extends estimates and local comparison reports. Estimates remain approximate.
The bundle-only external environmental provider is preserved by the documented
0.2.5.2 patch. Both Debian 12 architectures must pass build and clean-install
qualification before the existing release assets are replaced.

Exact source pins are in `.github/workflows/external-control-demo.yml`.
Final release notes will record qualified artifact hashes and workflow runs.
