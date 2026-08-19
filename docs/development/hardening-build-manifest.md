# OpenCPN 5.x hardening build manifest

**Manifest date:** 19 August 2026
**Repository:** `pob220/OpenCPN-Core-Hardening`  
**Baseline:** OpenCPN upstream `e87d2234509636d8e534f0278fb1c6ad8463eb2e`  
**Integration branch:** `hardening/5x-integration`  
**Integration commit:** `89e25705e8f1c7b180a8d7a8c067d629f391bf4d`

The integration commit above is the production/test state before importing
this documentation. Later documentation-only commits do not alter the tested
binary.

## Ordered integration commits

| Order | Integration commit | Change |
|---:|---|---|
| 1 | `d0914d89f3280dffe86e1a1a70be719c51aeafdb` | Deterministic blocking CTest/CI baseline |
| 2 | `dee3e806e0bd66c828ccf40e9fd2bdaf2703fc84` | #5311 N2K closing velocity units |
| 3 | `79081c5b6a2b006edd91e6348c29b3c207199fe7` | #5145 Signal K AIS type validation |
| 4 | `32e5ec57b89fc299ec10c28f049fe9937b706bac` | #5287 first-run initialization/readiness |
| 5 | `cce82e1073c0b473c4b972c679fa4a8ff22d3cec` | #5200/#5202 transactional route deletion |
| 6 | `1e186c590422cc56d6571f701d6e6c6fb2da3780` | #3851 SENC worker drain/lifetime |
| 7 | `8c8065b0bc522c31f57dbd056f53cc7121905b22` | Bounded Actisense framing |
| 8 | `bcc3febe5b811a9e81018ef238c0d7bbbb48572d` | N2K serial teardown/join |
| 9 | `30b2feb2b5997abe081d5d4cd2a9f53a51c261f1` | #4103 coherent route-leg transition state |
| 10 | `04cf1db5659cb4b43f86bfab97575d88696438e1` | Raster texture worker ownership |
| 11 | `22b2d91177786b8da830d956f668f1def4e927e2` | #5170 chart catalogue lifecycle |
| 12 | `a2bd936dea7d528dc8b02b671304fbde2e9ae530` | #5069 standalone AIS type-14 messages |
| 13 | `8949efae79d8d33255d5b67bf9ab19e67b1124b3` through `f5b10ac3870f4d905968448d553412256a094d51` | Optional chart-backed land/depth value service, immutable tiles, request servicing and plug-in-owned cache contract |
| 14 | `89e25705e8f1c7b180a8d7a8c067d629f391bf4d` | Correct wxWidgets route-mask log format for `long` elapsed time |

## Isolated review branches

| Branch | Purpose |
|---|---|
| `hardening/ci-test-baseline` | Test discovery and blocking deterministic CI |
| `fix/5311-n2k-closing-velocity` | N2K unit correction |
| `fix/5145-signalk-ais-validation` | Signal K boundary validation |
| `fix/5287-first-run-readiness` | First-run initialization |
| `fix/5200-routepoint-persistence` | Routepoint deletion transaction/invariants |
| `fix/3851-senc-chart-lifetime` | SENC worker lifetime |
| `hardening/n2k-actisense-input` | Actisense framing/input validation |
| `fix/4554-n2k-teardown` | N2K serial worker teardown |
| `fix/4103-route-transition` | Route-leg state calculation |
| `fix/texture-worker-ownership` | Raster texture worker ownership |
| `fix/plugin-chart-catalogue-lifecycle` | Plug-in chart catalogue mutation |
| `fix/5069-standalone-ais-type14` | AIS safety broadcast state/event/UI adapter |

Topic branches are based directly on the upstream baseline where practical.
Their commit IDs may differ from integration cherry-picks while the patch is
equivalent. Compare each topic branch to `master`, not to the integration tip.

## Local verification at the manifest commit

- CMake Debug configuration, wxGTK 3.2, OpenGL enabled.
- Complete `opencpn` target compiled and linked with warnings treated as errors.
- 71/71 tests labelled `deterministic` passed.
- 71/71 deterministic tests also passed in a separate no-OpenGL AddressSanitizer
  build. Leak detection was disabled for that run because the execution sandbox
  blocks LeakSanitizer's ptrace mechanism; this is an ASan result, not an LSan
  clean bill of health.
- The new standalone type-14 case passed independently before the full suite.
- Known host-dependent IPC/loopback tests are labelled `integration` and are not
  part of the deterministic result.
- The exact compatible weather-routing plug-in commit
  `f6891f8a78f49a59582eb320a67445ad498046f3` built against these headers and
  passed 158/158 tests.
- A live isolated-profile host run loaded that plug-in, registered and later
  unregistered its cache callbacks, and finished the built-in chart safety
  diagnostic with zero failures.
- A real CM93 2015 diagnostic requested 91 masks, built 146 base tiles,
  classified 245,426 cells, reused all masks and tiles on the second pass, and
  passed land and minimum-depth cases with zero failures.
- 18/18 focused hardening tests passed under AddressSanitizer after the chart
  integration. Leak detection was disabled for the sandbox limitation already
  noted above.
- Four loopback integration cases fail on this unprepared host because their
  expected network handles are absent. They are not included in the 71-test
  deterministic gate and are not concealed as passing tests.

## Linux developer package

The full default target, including bundled plug-ins, was built before CPack.
The generated artifact is:

```text
opencpn_5.15.0-1_x86_64.tar.gz
size: 27 MiB
sha256: ea30af0155ca88afa97f89b603aa0881f4f0db955f77044b6cef6f2960683d5b
```

The archive contains `bin/opencpn` and the normal OpenCPN shared data. It was
created on the audit Linux host and is not a portable, signed or official
distribution. Prefer rebuilding from the manifest on a matching target.

This is one Linux build result, not the cross-platform acceptance matrix.
Windows, macOS, Android/ARM and physical N2K/autopilot testing remain explicit
release gates in the implementation plan.
