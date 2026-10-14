# Debian 13 x86_64 installer — 14 October 2026

**Ready for Quinton's Debian 13 amd64 laptop.** The installer is
`opencpn-chart-aware-preview_5.15.0+chartaware.20261014.1+deb13_amd64.deb` (147,563,618 bytes, about 141 MiB).
See INSTALL.md for installation and testing commands.

SHA-256: `b890d15e0057d3822b7c73e016010093232c73d024f57984e6d837577cc46e53`.

## Source match

Built from the **12 October 2026** bundle attached to
https://github.com/pob220/OpenCPN-Core-Hardening/releases/tag/chart-aware-routing-preview-20260831.
The live release metadata and checksum-verified original archive were used,
rather than GitHub's original automatic release-tag source archive.

The entire core is **23c95ad379b83dd7a97e2edd7d6a859d49099a91**, unchanged.
This includes external control, core hardening, chart-safety geometry/proofs and
cardinal cross-track steering. It is the developer bundle's retained core,
not the separately evolving OpenCPN PR 5475.

The core, xWeatherRouting 1.30.0, xGRIB 0.3.9.0/generator 0.3.4,
Climatology 2026.2, Celestial Navigation 2.9.8.0 and modified o-charts provider
were built on Debian 13 with GCC 14.2.0, CMake 3.31.6 and wxWidgets 3.2.8.
All component checkouts remained clean. Polar 1.2.38.0 and the official
licensed o-charts helper/runtime retain the pinned vendor binaries; they were
included in the Debian 13 runtime checks. The pure-Python Scheduler, SDK and MCP
wheels are byte-identical to the published bundle's wheels.

Exact component revisions, runtime artifact checksums, packaging checksums and
98 audited x86_64 ELF paths are recorded in components.json and embedded in the
package. The application is installed under /usr/lib/opencpn-chart-aware and
uses an independent user profile and private plugin installation directory.
Its launcher retains OCPN_CHART_SAFETY_GEOMETRY_PROOF=1 and creates a new
loopback-only API token. HOME and normal OpenCPN installation paths are preserved.

## Validation

- 64 core regression tests passed, including 17 complete geometry-oracle tests
  with 70,000 comparisons, API/planning/environment/lifecycle checks and cardinal
  steering regression coverage.
- All 484 xWeatherRouting tests, 44 xGRIB tests and 3 Climatology tests passed.
- All 13 Celestial CTest targets and 9 isolated GUI suites passed. The initial
  Find GUI attempt timed out without a private D-Bus session; the same binary
  passed in its own D-Bus/display session. No plugin source fix was made.
- 8 first-run/profile-isolation setup tests passed.
- Clean APT installation succeeded in a fresh Debian 13 runtime container with
  declared dependencies and GUI test utilities, before ordinary OpenCPN was added.
  No compiler or development packages were installed in that runtime container.
- Fresh software rendering, OpenGL with Mesa software rendering, and software
  recovery passed startup, all-six-plugin initialization and clean shutdown.
- Environmental/planning provider registration, authenticated API, Python SDK,
  Scheduler entry point, MCP discovery/status and packaged GRIB merging passed.
- Installing alongside Debian's ordinary OpenCPN, reinstalling the preview and
  removing it preserved the ordinary binary and the private profile/token.
- The installer and all source archives have matching SHA-256 checksums.

The complete clean-runtime log is qualification-debian13-amd64.log. Detailed
build/test logs and XML are retained in the parent artifact directory.
Container checks do not establish physical laptop GPU behavior, chart licensing,
dongle/TPM access or actual licensed-chart route results. Those remain Quinton's
native testing. No licensed charts, forecasts, entitlements, semantic caches or
large optional celestial kernels are included. Build and qualification did not install anything into the host desktop.

## Source and rebuild

opencpn-core-23c95ad379b8-source.tar.gz contains the complete matching core source.
debian13-packaging-source.tar.gz contains the packaging overlay and scripts.
All open-source plugin and generator sources are pinned to public immutable
revisions in components.json. The overlay's older DEVELOPMENT.md documents its
historical chart-only baseline; use the following sequence for this full-core build.

1. Clone pob220/OpenCPN-Core-Hardening and check out the exact core revision above.
   Extract the packaging overlay into that checkout.
2. Build distribution/Dockerfile as opencpn-chart-aware-builder:trixie. The build
   directory must be writable by UID 1000, the image's builder user.
3. Run distribution/fetch-components.py WORK/inputs with GIT_LFS_SKIP_SMUDGE=1.
   It fetches and verifies the pinned source/runtime artifacts.
4. Download de440s.bsp from the Celestial eclipse-data-2026.1 release into
   WORK/inputs/celestial/eclipse/data for tests. Its SHA-256 must be
   c1c7feeab882263fc493a9d5a5b2ddd71b54826cdf65d8d17a76126b260a49f2.
   The installer assembly explicitly excludes optional .bsp/.bpc/LOLA packs.
5. Download and checksum-verify the original 2026-10-12 Debian 12 x86_64 bundle,
   then extract it into INPUTS (alongside WORK). Its original archive SHA-256 is
   5a9fd34fc1f466bf2e9b3cbfc876ceeab62b00b68eb9c3c425538ae7428061b8.
   Assembly uses its three pure-Python wheels and documentation.
6. In the Debian 13 builder, mount the checkout read-only at /src and WORK at
   /work. Run distribution/build-core.sh /work, then with OCPN_TARGET=trixie run
   distribution/build-plugins.sh /work. Every Celestial GUI suite needs its own
   D-Bus/display session as the scripts specify.
7. Also mount INPUTS read-only at /inputs and run
   python3 /src/distribution/make-package.py /work. The output is /work/packages.
8. In a fresh debian:trixie container, mount /src and /packages read-only and run
   distribution/qualify.sh /packages/EXACT-PACKAGE.deb. This installs, exercises,
   reinstalls and removes the application inside that disposable container.

See the retained core-additional-tests, core-environment-tests and
core-route-leg-tests logs for the extra core filters exercised in this build.
