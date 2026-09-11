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

The macOS upstream dependency archive is pinned to SHA-256
`1a9422ee632effb0a47eb5fbaaf2effaf0c26b321a8155f824453fbafb058dc6`.
Arch package versions and Flatpak SDK commits are recorded in the evidence.

## Targeted retries

Use the workflow's `platform_group` dispatch input: `linux`, `debian`, `ubuntu`,
`native`, `windows`, `macos`, `portability`, `arch`, `flatpak`, or `all`.
Corrective commits may use `[skip ci]`, followed by a
dispatch of only the affected group. This preserves evidence from unaffected
jobs and avoids rebuilding every platform for a packaging-only fix.

Initial setup failures were missing `libunarr-dev` on Debian 13 (unused by the
core build), missing `gnupg` for Jammy PPA registration, explicit CA configuration
for minimal Debian images, missing Windows Gettext PATH, and macOS dependency
extraction through Homebrew symlinks. Clang's existing missing-override warnings
remain visible but are not errors in the macOS probe. None of these changes
alters Weather Routing code. The Apple CMake configuration also preserves
caller-supplied C++ flags rather than silently replacing them.

The initial Windows job compiled successfully but produced no GoogleTest XML.
It is **not** counted as a test pass. Windows batch checks now reject negative
process exit statuses, use the native dependency path for the test executable,
and require a nonempty, failure-free XML report containing the API and
chart-safety suites. macOS and Arch probes validate their XML reports too.

Apple Clang also identified a C++20-only structured-binding lambda capture in
the environmental request encoder. It now uses ordinary references with the
same value conversion, compatible with the declared C++17 standard. A strict
local Clang C++17 syntax check passes. GNU Patch is installed on macOS, and
ShapeFileCpp staging/patch commands now propagate failures instead of hiding
them behind a second `execute_process` command. The four dependency patches
were checked locally against a fresh source copy.

The GRIB zoom diagnostic now keeps both conditional values as `wxString`
before formatting: this avoids Apple's writable-string conversion error
without changing routing or rendering behaviour. A local Clang C++17 check
with `-Werror -Wwritable-strings` passes.

The shapelib and RapidJSON patch commands also ran concurrently with an echo
command in a [CMake pipeline](https://cmake.org/cmake/help/latest/command/execute_process.html).
That could close the patch output pipe early and
hid patch failures; an Arch retry exposed a truncated dependency CMake file.
Each patch now runs alone, with its result checked. Already-applied patches
are recognised by a non-mutating reverse dry run, and incompatible sources
still fail. Regression tests cover fresh application, repeated configuration,
and failure propagation. All three pinned shapelib patches were applied twice
locally and the resulting dependency configured successfully.

Arch trusts only its exact ephemeral CI checkout for Git commands, including
the version queries made by CMake. No wildcard safe-directory exception is used.

Flatpak prefetches nlohmann-json 3.12.0 at commit
`55f93686c01528224f448c19128836e7df245f72` before entering its offline build
sandbox; CMake must not try to fetch this dependency during the build.
Its test phase uses explicit GoogleTest commands and XML validation, with an
empty [`test-rule`](https://docs.flatpak.org/en/latest/flatpak-builder-command-reference.html)
to avoid Flatpak's default `make check` target, which this
CMake project does not define. Tests remain enabled; missing or failed reports
are fatal. The generated manifest's isolation, source pins and test invocation
are covered by a local regression test.
SDK evidence uses `flatpak list --columns=ref` followed by
`flatpak info --show-commit` for each installed runtime. Ubuntu's supported
Flatpak client has no `list` column named `commit`; that metadata error had
occurred after a successful ARM64 build and all 36 tests in run 34603398453.
That failed overall job is not counted as a completed qualification.

## Independently checked evidence

The following results were checked from downloaded artifacts, not just the
workflow's green/red status. This table records targeted runs; it does not
claim that all platforms came from one revision or that native core probes
are installable bundles.

All ten qualification targets now have successful complete jobs. Each retained
core XML report was downloaded and validated independently. The ten local
qualification-tool regression tests also pass. Existing public preview assets
and the user's installed OpenCPN were not modified.

| Target | Run | Checked result |
| --- | --- | --- |
| Windows x86 core | [34600329911](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34600329911) | 36 focused core tests passed, XML independently validated |
| Ubuntu 22.04 x86_64 bundle | [34599029187](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34599029187) | 36 core and 222 Weather Routing tests passed; isolated bundle qualification passed; 91 ELF files architecture-checked |
| Ubuntu 24.04 x86_64 bundle | [34598503167](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34598503167) | 36 core and 222 Weather Routing tests passed; isolated bundle qualification passed; 96 ELF files architecture-checked |
| Debian 13 x86_64 bundle | [34599811275](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34599811275) | 36 core and 222 Weather Routing tests passed; isolated bundle qualification passed; 98 ELF files architecture-checked |
| Debian 13 ARM64 bundle | [34599811275](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34599811275) | 36 core and 222 Weather Routing tests passed; isolated bundle qualification passed; 98 ELF files checked as AArch64 |
| macOS Apple Silicon core | [34601989436](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34601989436) | 36 focused core tests passed; XML independently validated; executable checked as Mach-O ARM64 |
| macOS Intel core | [34601989436](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34601989436) | 36 focused core tests passed; XML independently validated; executable checked as Mach-O x86_64 |
| Arch Linux x86_64 core | [34602485919](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34602485919) | 47 core/renderer tests passed; XML independently validated; native x86_64 ELF and no unresolved `ldd` dependencies |
| Flatpak x86_64 core | [34605890322](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34605890322) | 36 focused core tests passed inside the SDK sandbox; XML independently validated; native x86_64 ELF and SDK commit evidence retained |
| Flatpak ARM64 core | [34605890322](https://github.com/pob220/OpenCPN-Core-Hardening/actions/runs/34605890322) | 36 focused core tests passed inside the SDK sandbox; XML independently validated; native AArch64 ELF and SDK commit evidence retained |

Some of these older runs contain failed jobs for other targets which were
subsequently retried separately. Arch run 34600696702 compiled and passed 47
tests, but its source-revision evidence step failed; it is not a successful
complete qualification run.
