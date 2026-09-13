# Qualified preview refresh: Weather Routing 1.17.5

Developer preview for testing the hardened OpenCPN external-control stack and chart/depth-aware Weather Routing integration. Experimental software: not a substitute for normal navigation checks.

## 13 September 2026 refresh

- **xWeatherRouting 1.17.5.0**: adds bounded recovery around continental obstructions, actionable land/depth prerequisite errors, and visible progress for ordinary computations. The progress window identifies each route and its current search stage, counters and worker-update age; Hide keeps work running, and Stop explicitly cancels it. Failed headless runs no longer present provisional geometry or arrival times as completed results. GPX exports handle names containing slashes. Existing safety enforcement, weather fallback, resource ceilings and earlier fixes are retained.
- **xGRIB 0.2.5.2 and Environmental GRIB Generator 0.1.8**: add live offline size estimates, measured GRIB totals and local size-comparison reports, including extended Copernicus estimates. Estimates are approximate. This bundle retains its external environmental-acquisition provider.
- **Celestial Navigation 2.8.5.4**: separates Find-popup position editing from explicit estimated-Hs copying, adds independent reset/cancel controls and observed-altitude display, and clarifies the time-panel toggle. Ambiguous lunar UTC/position solutions now default according to valid dead-reckoning position proximity, while retaining explicit selection and all available branches. Earlier lunar/observer-astrometry and coastal numerical fixes are retained.
- Climatology **1.6.39 / dataset 2026.2**, Polar **1.2.38.0**, and the modified o-charts semantic provider with its official **2.2.1** helper/runtime were checked and retained.

The existing hardened OpenCPN core is unchanged. The clean, pinned `pob220/weather_routing_pi` source is built under the isolated **xWeatherRouting** identity. No tester-local changes are included. xGRIB's existing public Alpha build/publishing setup is unchanged; its provider integration is a documented, bundle-only patch against the pinned 0.2.5.2 source.

## Download and install

Choose the **2026-09-13** archive for **Debian 12 x86_64** or **Debian 12 ARM64**, matching your machine, and its `.sha256` file. Verify the checksum, extract the archive, and follow its `README.md` and isolated installer instructions. Extract into a fresh directory. Do not mix files from an older extracted bundle with this refresh.

This is the broader developer bundle, including external-control, SDK/MCP and Scheduler tools. For the focused Debian 13 installer without the added external-control service or Vulkan experiment, use the separate [Chart-Aware preview](https://github.com/pob220/OpenCPN-Chart-Aware/releases/tag/debian13-preview-20260910).

## Exact sources

- Bundle assembly: `5b6a425edf8a19ad0425160cb312b95a051f1915` on `preview/weather-routing-1.17.5-20260913`.
- Unchanged core artifacts: successful [core build 34130966851](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34130966851); the full core revision is recorded inside each bundle's `COMPONENTS.md`.
- Weather Routing 1.17.5.0: `39a8f88e2fa2528f603b113443e446b22ba7e481`.
- xGRIB 0.2.5.2: `f5e1ea1019f37af4d8d8e951f43213e8d122f96d`, plus the assembly revision's `ci/external-control-demo/xgrib-0.2.5.2-provider.patch`, preserving the provider from `446814bafe6edfeaf23fadc3f478864f56d81362`.
- Generator 0.1.8: `bf650d8960423461f607f9d96edb257e1092a7b9`.
- Climatology: `cd00282e6ea2784a6d78ccfe47fed713269ad87e`.
- o-charts: `b369c4cae0e84ea43847f64a26cb107ae1ffebe5`.
- Celestial Navigation 2.8.5.4: `a003cf7d36880ca38d15a172bcde48ef5c3fa421`.

## Qualification

Both Debian 12 x86_64 and ARM64 candidates passed [qualification run 34753111800](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34753111800): Weather Routing **238 tests**, xGRIB **29 tests**, Climatology **3 tests**, and all **7 Celestial Navigation CTest targets**, on each architecture. The opt-in Find, lunar and coastal GUI regression suites also passed in separate Xvfb processes on both architectures.

Bundle checks covered clean isolated installation, all ten plugin binaries, provider/API checks, GUI startup and lifecycle; x86_64 also passed the full MCP smoke qualification. Downloaded archives were independently checked against their SHA-256 files, internal checksums, component pins and embedded plugin versions. Platform qualification logs accompany this release. The previously published Celestial Navigation 2.8.5.4 source and its corrected qualification checks are retained.

SHA-256:

- x86_64: `1a66b859255fd30ec9f6e11b74237d09c8ae36d1183169e3b6e71c6743a35bce`
- ARM64: `952e802666d8887923a9ecd24557b1f5feb95e29191ce4d660fb21d405e62081`

No licensed charts, entitlements or semantic-atlas cache are included. Real licensed-chart access, chart coverage, GPU/hardware behavior and route suitability require testing on the user's own system. Celestial's optional DE440s and lunar-orientation/LOLA data remain opt-in and are not embedded.

## Routing validation and remaining limits

Native Linux and Debian 12 testing passed all **238 tests**. Controlled Provincetown–Lizard routes with land detection, the Nicholson 35 polar, a 15-day ECMWF forecast, offline tidal/seasonal currents and Climatology wind fallback completed at 100% effort with 3-hour/10-degree and 6-hour/20-degree settings. The 0.5-degree forecast with 3-hour/10-degree settings exhausted 100%, then completed at the 150% tier when allowed 200%. More effort can help; it does not guarantee completion. These concurrent measurements are functional tests, not controlled speed benchmarks or navigation recommendations.

Five existing no-land Atlantic routings and the forecast/Climatology handoff control preserved their geometry exactly. The CM93 Vava’u control also matched 1.17.4; depth, missing-coverage, unsupported-host and land-endpoint controls behaved as expected. Quinton’s exact saved island case remains unavailable, so these controls do not establish the cause of his earlier reported land crossing. Physical graphics drivers, all chart providers and every reported routing case have not been qualified.
