Developer preview for testing the hardened OpenCPN external-control stack and chart/depth-aware Weather Routing integration. Experimental software: not a substitute for normal navigation checks.

## 12 September 2026 refresh

- **xWeatherRouting 1.17.4.0**: fixes a shared engine bug that could discard a complete destination connection when further exploration exhausted the search budget. Independent validation and safety checks remain in place. Startup logs now identify the plugin version, API target and process bitness; Windows address-space diagnostics are also corrected. Earlier chart-policy, longitude, coverage, timeout, Climatology and lifetime fixes are retained.
- **xGRIB 0.2.5.2 and Environmental GRIB Generator 0.1.8**: add live offline size estimates, measured GRIB totals and local size-comparison reports, including extended Copernicus estimates. Estimates are approximate. This bundle retains its external environmental-acquisition provider.
- **Celestial Navigation 2.8.5.3**: updates the earlier 2.8.4 baseline with the newer workflow, lunar/observer-astrometry and coastal numerical fixes, including the latest numerical audit corrections.
- Climatology **1.6.39 / dataset 2026.2**, Polar **1.2.38.0**, and the modified o-charts semantic provider with its official **2.2.1** helper/runtime were checked and retained.

The existing hardened OpenCPN core is unchanged. The clean, pinned `pob220/weather_routing_pi` source is built under the isolated **xWeatherRouting** identity. No tester-local changes are included. xGRIB's existing public Alpha build/publishing setup is unchanged; its provider integration is a documented, bundle-only patch against the pinned 0.2.5.2 source.

## Download and install

Choose the **2026-09-12** archive for **Debian 12 x86_64** or **Debian 12 ARM64**, matching your machine, and its `.sha256` file. Verify the checksum, extract the archive, and follow its `README.md` and isolated installer instructions. The filenames retain the same date as the earlier refresh: compare the checksums below and extract into a fresh directory. Do not mix files from an older extracted bundle with this refresh.

This is the broader developer bundle, including external-control, SDK/MCP and Scheduler tools. For the focused Debian 13 installer without the added external-control service or Vulkan experiment, use the separate [Chart-Aware preview](https://github.com/pob220/OpenCPN-Chart-Aware/releases/tag/debian13-preview-20260910).

## Exact sources

- Bundle assembly: `4e4df2ef39e75fb1b6898fc0f0e2bc2100c46a00` on `preview/weather-routing-1.17.4-20260912`.
- Unchanged core artifacts: successful [core build 34130966851](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34130966851); the full core revision is recorded inside each bundle's `COMPONENTS.md`.
- Weather Routing 1.17.4.0: `f33a53ec536f6be1849eb289e27ce6c4f3dda1c4`.
- xGRIB 0.2.5.2: `f5e1ea1019f37af4d8d8e951f43213e8d122f96d`, plus the assembly revision's `ci/external-control-demo/xgrib-0.2.5.2-provider.patch`, preserving the provider from `446814bafe6edfeaf23fadc3f478864f56d81362`.
- Generator 0.1.8: `bf650d8960423461f607f9d96edb257e1092a7b9`.
- Climatology: `cd00282e6ea2784a6d78ccfe47fed713269ad87e`.
- o-charts: `b369c4cae0e84ea43847f64a26cb107ae1ffebe5`.
- Celestial Navigation 2.8.5.3: `49ecbe0c8b68e486595c86966054a741a97f6475`.

## Qualification

Both Debian 12 x86_64 and ARM64 bundles passed [build and qualification run 34697811656](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34697811656): Weather Routing **233 tests**, xGRIB **29 tests**, Climatology **3 tests**, and all **6 Celestial Navigation CTest targets**, on each architecture. Clean isolated installation, all ten plugin binaries, provider/API checks, GUI startup and lifecycle checks passed; x86_64 also passed the full MCP smoke qualification.

The downloaded archives were independently checked against their SHA-256 files, internal asset checksums, component pins and embedded routing/xGRIB package versions. Platform qualification logs accompany the release. These are automated smoke checks, not full real-world routing or celestial-accuracy certification.

SHA-256:

- x86_64: `7b875ee7958639fdd8e5ecef76c27402d5f43359dee78e3e19bf917148990cba`
- ARM64: `4f5f9009b17ecb8c1bd18840116a39fa4e6f1d065faf2245a427513cf80eb461`

No licensed charts, entitlements or semantic-atlas cache are included. Real licensed-chart access, chart coverage, GPU/hardware behavior and route suitability require testing on the user's own system. Celestial's optional DE440s and lunar-orientation/LOLA data remain opt-in and are not embedded.


## Routing limitation still under investigation

The 1.17.4 completion regression passes, but a longer controlled route using synthetic weather still reaches the bounded search limit on 64-bit Linux without memory exhaustion. This refresh does not claim to fix every reported long-route, graphics or Windows problem. The new Windows monitoring code still needs native Windows validation; these preview packages are qualified on Linux.
