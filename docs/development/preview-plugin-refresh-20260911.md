Developer preview for testing the hardened OpenCPN external-control stack and chart/depth-aware Weather Routing integration. Experimental software: not a substitute for normal navigation checks.

## 11 September 2026 refresh

- **xWeatherRouting 1.17.2.0**: fixes Climatology wind-atlas initialization and longitude differences in course calculations; retains the 1.17.1 route-table lifetime fix.
- **xGRIB 0.2.5.1**: updates the GRIB viewer/generator integration and editable area presets, while preserving this bundle's external environmental-acquisition provider. Generator remains **0.1.7**.
- **Celestial Navigation 2.8.5.3**: updates the earlier 2.8.4 baseline with the newer workflow, lunar/observer-astrometry and coastal numerical fixes, including the latest numerical audit corrections.
- Climatology **1.6.39 / dataset 2026.2**, Polar **1.2.38.0**, and the modified o-charts semantic provider with its official **2.2.1** helper/runtime were checked and retained.

The existing hardened OpenCPN core is unchanged. The clean, pinned `pob220/weather_routing_pi` source is built under the isolated **xWeatherRouting** identity. No tester-local changes are included. xGRIB's existing public Alpha build/publishing setup is unchanged; its provider integration is a documented, bundle-only patch against the pinned 0.2.5.1 source.

## Download and install

Choose the **Debian 12 x86_64** or **Debian 12 ARM64** archive matching your machine, and its `.sha256` file. Verify the checksum, extract the archive, and follow its `README.md` and isolated installer instructions. Do not mix files from an older extracted bundle with this refresh.

This is the broader developer bundle, including external-control, SDK/MCP and Scheduler tools. For the focused Debian 13 installer without the added external-control service or Vulkan experiment, use the separate [Chart-Aware preview](https://github.com/pob220/OpenCPN-Chart-Aware/releases/tag/debian13-preview-20260910).

## Exact sources

- Bundle assembly: `f2d8d0413360ab0c98e1a2121378be94c9f0b706` on `preview/plugin-refresh-20260911`.
- Unchanged core artifacts: successful [core build 34130966851](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34130966851); the full core revision is recorded inside each bundle's `COMPONENTS.md`.
- Weather Routing 1.17.2.0: `49036e934a4102d8426819f92f3cc5f61ead717f`.
- xGRIB 0.2.5.1: `b4fd23d75df24e6c8bb6c3ac2ac502dd8e15b637`, plus the assembly revision's `ci/external-control-demo/xgrib-0.2.5.1-provider.patch`, preserving the provider from `446814bafe6edfeaf23fadc3f478864f56d81362`.
- Generator 0.1.7: `6156c997191ffbfa9fba39385e84fa06a2fed353`.
- Climatology: `cd00282e6ea2784a6d78ccfe47fed713269ad87e`.
- o-charts: `b369c4cae0e84ea43847f64a26cb107ae1ffebe5`.
- Celestial Navigation 2.8.5.3: `49ecbe0c8b68e486595c86966054a741a97f6475`.

## Qualification

Both Debian 12 x86_64 and ARM64 bundles passed [build and qualification run 34583066075](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34583066075): Weather Routing **222 tests**, xGRIB **27 tests**, Climatology **3 tests**, and all **6 Celestial Navigation CTest targets**, on each architecture. Clean isolated installation, all ten plugin binaries, provider/API checks, GUI startup and lifecycle checks passed; x86_64 also passed the full MCP smoke qualification.

The downloaded archives were independently checked against their SHA-256 files, internal asset checksums, component pins and embedded routing/xGRIB package versions. Platform qualification logs accompany the release. These are automated smoke checks, not full real-world routing or celestial-accuracy certification.

SHA-256:

- x86_64: `01c5ab791cfc9db80a306a3bbeb58ea62861c47f0648e20b9e6420c2b6c8e6b7`
- ARM64: `e6078dd743899dbf26495b7e7ceeefc3e9900cb716798a7bea6907f886745d7f`

No licensed charts, entitlements or semantic-atlas cache are included. Real licensed-chart access, chart coverage, GPU/hardware behavior and route suitability require testing on the user's own system. Celestial's optional DE440s and lunar-orientation/LOLA data remain opt-in and are not embedded.
