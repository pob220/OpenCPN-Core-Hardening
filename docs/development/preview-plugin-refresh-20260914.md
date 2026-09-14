Developer preview for testing the hardened OpenCPN external-control stack and chart/depth-aware Weather Routing integration. Experimental software: not a substitute for normal navigation checks.

## 14 September 2026 refresh

- **xWeatherRouting 1.17.7.0** includes two selectable native C++ engines. **Main remains the default** and upgrades preserve existing route and last-used settings. **Quick** is a separate bounded beam-search engine with its own tuning and a configurable 256 MiB default per-route search-storage ceiling.
- **Shoreline detail 0–4** selects Crude, Low, Intermediate, High or Full GSHHG 2.3.7 data. All five datasets are included for offline use and were independently checked against their embedded compressed and uncompressed hashes. Main starts at Full on a fresh install; Quick starts at Crude and each engine remembers its choice.
- With enforced chart safety on this modified core, chart and depth evidence remain authoritative and the shoreline control becomes an editable, separately saved **Scout shoreline resolution**, initially Crude. Stock OpenCPN installations retain the five ordinary shoreline choices.
- Main gains a small bounded final-arrival allowance when a route reaches the normal work limit close to its destination. The normal motion checks, independent final validation, fallback allocations and retained-state limits remain in force.
- Main can retry a fully land-rejected long-step layer at the minimum routing step, with strict layer and generated-state ceilings. The normal path is unchanged when it advances, both sides of an obstruction remain eligible, and active geometry/boundary/cyclone limits disable this guidance.
- Max Diverted Course is now identified as a hard route-geometry limit independent of Max Search Angle. Failed routes report when a narrower diverted-course limit may be excluding the sampled detour.
- The Advanced page now uses balanced columns and a separate Cyclone avoidance group. All earlier Basic and Advanced controls remain available; saved values are not reset. Presets change settings only after **Reset engine to preset** and an Apply/Cancel preview.
- xGRIB **0.2.5.2**, Environmental GRIB Generator **0.1.8**, Celestial Navigation **2.8.5.4**, Climatology **1.6.39 / dataset 2026.2**, Polar **1.2.38.0**, and the modified o-charts semantic provider with its official **2.2.1** helper/runtime are retained.

Quick shares Main's physical propagation and independent final validation, but prunes the search much more aggressively and can miss a feasible or faster route that Main finds. Its memory setting limits tracked Quick search storage; it is not a cap on total OpenCPN memory and does not establish that the Windows 32-bit process fits its address-space limit.

The existing hardened OpenCPN core is unchanged. The clean, pinned `pob220/weather_routing_pi` source is built under the isolated **xWeatherRouting** identity. No tester-local changes are included.

## Download and install

Choose the **2026-09-14** archive for **Debian 12 x86_64** or **Debian 12 ARM64**, matching your machine, and its `.sha256` file. Verify the checksum, extract into a fresh directory, and follow its `README.md` and isolated installer instructions. Do not mix files from an older extracted bundle with this refresh.

This is the broader developer bundle, including external-control, SDK/MCP and Scheduler tools. For the focused Debian 13 installer without the added external-control service or Vulkan experiment, use the separate [Chart-Aware preview](https://github.com/pob220/OpenCPN-Chart-Aware/releases/tag/debian13-preview-20260910).

## Exact sources

- Bundle assembly: `16405a760c0dd9bd61a2e4e4ca49a25674120e8e` on `preview/weather-routing-1.17.7-20260914`.
- Unchanged core artifacts: successful [core build 34130966851](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34130966851); the full core revision is recorded inside each bundle's `COMPONENTS.md`.
- Weather Routing 1.17.7.0: `9bf27ef4537c1a56680267b75851a08340d30efd`.
- xGRIB 0.2.5.2: `f5e1ea1019f37af4d8d8e951f43213e8d122f96d`, plus the assembly revision's `ci/external-control-demo/xgrib-0.2.5.2-provider.patch`.
- Generator 0.1.8: `bf650d8960423461f607f9d96edb257e1092a7b9`.
- Climatology: `cd00282e6ea2784a6d78ccfe47fed713269ad87e`.
- o-charts: `b369c4cae0e84ea43847f64a26cb107ae1ffebe5`.
- Celestial Navigation 2.8.5.4: `a003cf7d36880ca38d15a172bcde48ef5c3fa421`.

## Qualification

Both Debian 12 x86_64 and ARM64 candidates passed [qualification run 34848829610](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34848829610): Weather Routing **288 tests**, xGRIB **29 tests**, Climatology **3 tests**, and all **7 Celestial Navigation CTest targets**, on each architecture. The opt-in Find, lunar and coastal GUI regression suites also passed in separate Xvfb processes on both architectures.

Bundle checks covered clean isolated installation, all ten plugin binaries, provider/API checks, GUI startup and lifecycle; x86_64 also passed the full MCP smoke qualification. Downloaded archives were independently checked against their SHA-256 files, internal checksums, component pins, ELF architecture, embedded plugin versions, and all five GSHHG datasets. Platform qualification logs accompany this release.

SHA-256:

- x86_64: `60b82d168e4b33d626abc3a975ab2e3433037f8100337fc0dd04ba4c7d4f0bfb`
- ARM64: `306119a425e256979a531b13f9bcca43809ceeb787eb9d56df23352cf58d5530`

No licensed charts, entitlements or semantic-atlas cache are included. Real licensed-chart access, chart coverage, GPU/hardware behavior and route suitability require testing on the user's own system.

## Routing evidence and limits

In controlled development runs of the same Quick policy, a completed Provincetown–Lizard case using Main at 6h/20° took 324.780 seconds and peaked at 1,594.4 MiB process RSS; Quick completed in 69.476–77.886 seconds and peaked at 557.7–557.8 MiB. The two Quick memory settings tested, 96 MiB and 256 MiB, produced the same independently validated route and used only 0.368 MiB of tracked search storage. These are observations on one Linux machine under controlled inputs, not guaranteed speed or memory figures. The engines use different search policies.

The 1.17.7 regression set also covers configuration migration, independent engine settings, bounded memory/work exhaustion, authoritative chart rejection, coastal egress, cancellation, all five offline shoreline levels, and preservation of the earlier Atlantic controls. Main remains the broader solver; Quick can trade route quality and solvability for speed and lower retained state. Neither these Linux bundles nor the separate Linux 32-bit engine test constitute Windows runtime qualification.
