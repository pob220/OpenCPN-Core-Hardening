# Desktop preview platform expansion — 11 September 2026

Work branch: `preview/platform-expansion-20260911`.
Workflow: `.github/workflows/preview-platform-expansion.yml`.

This extends the broad Core-Hardening developer preview, not the separately
maintained minimal Debian 13 Chart-Aware installer. Android and iOS are excluded.
The already published Debian 12 x86_64 and ARM64 archives remain unchanged.

## Qualification stages

| Target | Initial CI stage | Still required before a complete preview can be published |
| --- | --- | --- |
| Debian 13 x86_64 / ARM64 | Core, pinned plugins, bundle assembly, isolated installation and GUI smoke test | Passing evidence, clean-host prerequisites and chart/provider testing |
| Ubuntu 22.04 / 24.04 x86_64 | Same full Linux qualification | Distribution-specific dependency and helper compatibility; clean-host test |
| Windows x86 | Core build, architecture/dependency inspection and focused tests | Native plugin/helper builds, isolated installer/profile, current MSVC runtime and Windows token-file ACLs |
| macOS Intel / Apple Silicon | Upstream wx ABI dependency bundle, core build and focused tests | Plugin/helper builds, relocatable app, isolated profile, signing/notarisation and chart licensing tests |
| Arch Linux x86_64 | Clean rolling-distribution core build and focused tests | Native plugin/helper builds, dependency metadata and isolated package installation |
| Flatpak x86_64 / ARM64 | Pinned upstream SDK manifest with a separate preview app ID, core build and tests | All plugin/helper modules, Scheduler integration, sandbox permissions and chart/device access tests |

Core-only evidence is explicitly named `NOT-A-BUNDLE`. The workflow has read-only
repository permissions and never uploads assets to a release. A passing core
compile alone is not a claim that the complete bundle works on that platform.

## Invariants

- The core implementation initially matches the published binary revision
  `1040c48f17dc2adf9ee27ebdc2cfd82f3815d3fd`; source comparison found only
  packaging, CI and documentation changes at the start of this branch.
- Reuse the published, exact plugin pins, including xWeatherRouting 1.17.2.
  Do not substitute a developer's uncommitted working tree.
- Existing OpenCPN installations, profiles, charts and entitlement data must
  remain untouched. No activation of routes or autopilot output during tests.
- Software rendering remains the default. OpenGL is built; Vulkan is optional
  on supported Linux builds and is not enabled by default. Native macOS and
  Windows probes explicitly disable the Linux-specific Vulkan presenter.
- Windows must not redistribute the historical app-local MSVC DLLs which
  shadow a current system runtime. The probe sets `OCPN_BUNDLE_VCDLLS=OFF`.
- No licensed chart data, tokens or user credentials are packaged or uploaded.

The Linux candidate currently uses pinned matching-architecture Debian 12 Polar
and official o-charts helper archives. These are compatibility candidates, not
proof of portability. Qualification must reject unresolved libraries; rebuild
Polar or select a verified target-native helper if needed. Do not rename a
Bookworm binary to imply it was built on another distribution.

The macOS upstream dependency archive is recorded by SHA-256 for diagnosis; its
expected checksum must be pinned before any distributable macOS bundle is made.
Arch package versions and Flatpak SDK commits are recorded in the evidence.

## Targeted retries

Use the workflow's `platform_group` dispatch input: `linux`, `native`, `macos`,
`portability`, or `all`. Corrective commits may use `[skip ci]`, followed by a
dispatch of only the affected group. This preserves evidence from unaffected
jobs and avoids rebuilding every platform for a packaging-only fix.

Initial setup failures were missing `libunarr-dev` on Debian 13 (unused by the
core build), missing `gnupg` for Jammy PPA registration, explicit CA configuration
for minimal Debian images, missing Windows Gettext PATH, and macOS dependency
extraction through Homebrew symlinks. Clang's existing missing-override warnings
remain visible but are not errors in the macOS probe. None of these changes
alters Weather Routing code.
