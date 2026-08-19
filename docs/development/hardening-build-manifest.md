# OpenCPN 5.x hardening build manifest

**Manifest date:** 19 August 2026
**Repository:** `pob220/OpenCPN-Core-Hardening`  
**Baseline:** OpenCPN upstream `e87d2234509636d8e534f0278fb1c6ad8463eb2e`  
**Integration branch:** `hardening/5x-integration`  
**Integration commit:** `f87ee79311e62b09983c5006fe474012cf07da3a`

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
| 15 | `2becfdb75` | Keep isolated-profile plug-in discovery independent of user data paths |
| 16 | `5d41176cc` | Reject malformed CM93 dictionary state safely |
| 17 | `07f9e0d2b` | Correct wxWidgets chart-service timing format arguments |
| 18 | `9ab548c99` | Recover CM93 boundary omissions conservatively and invalidate v1 caches |
| 19 | `a77db92ba` | Honour the caller's segment-safety service time budget |
| 20 | `f87ee7931` | Recover confirmed CM93 depth seams from four authoritative probes, invalidate v2 caches, and add route-endpoint/seam canaries |

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
- 84/84 tests labelled `deterministic` passed.
- The earlier 71-test deterministic subset passed in a separate no-OpenGL
  AddressSanitizer build. Leak detection was disabled for that run because the
  execution sandbox blocks LeakSanitizer's ptrace mechanism; this is an ASan
  result, not an LSan clean bill of health. The 13 subsequently added cases have
  ordinary Debug coverage but have not all been repeated in that ASan tree.
- The new standalone type-14 case passed independently before the full suite.
- Known host-dependent IPC/loopback tests are labelled `integration` and are not
  part of the deterministic result.
- The exact compatible weather-routing plug-in commit
  `b411e62ed8059549925b2ec66f986cb1ca586db9` built against these headers and
  passed 175/175 tests.
- A live isolated-profile host run loaded that plug-in, registered and later
  unregistered its cache callbacks, and finished the built-in chart safety
  diagnostic with zero failures.
- Real CM93 diagnostics passed land, minimum-depth, Holyhead/Foyle endpoint and
  known tile-seam canaries with zero failures. The former seam tile made four
  independent authoritative probes, recovered all four confirmed-water cells,
  and left no falsely unknown cells. Direct chart evidence at the diagnostic
  points was 15 m at Holyhead, 20 m at the seam and 10 m at Foyle.
- The practical xGRIB/polar acceptance ran seven departure candidates across a
  +/-3 hour window from Holyhead to the Mouth of Lough Foyle with 5 m minimum
  depth, 0.4 NM land margin and no 128-hour ceiling. Six candidates completed
  and all six passed final chart-safety validation. The seventh reached the
  configured generated-state bound during fallback; the host exited normally.
  Runtime was 15 minutes 8 seconds on the audit host.
- 18/18 focused hardening tests passed under AddressSanitizer after the chart
  integration. Leak detection was disabled for the sandbox limitation already
  noted above.
- Four loopback integration cases fail on this unprepared host because their
  expected network handles are absent. They are not included in the 71-test
  deterministic gate and are not concealed as passing tests.

## Linux developer package

The full Debug target, bundled standard plug-ins and the exact compatible
weather-routing plug-in were installed into an isolated prefix. Assertions and
debug symbols are intentionally retained. The generated tester artifact is:

```text
OpenCPN-5.15.0-core-hardening-chart-aware-debug-arch-x86_64.tar.gz
size: 135 MiB (141,276,245 bytes)
sha256: 60ecfce4ff08cbe4a35ad6e0c417c31c05a0b24c57905d54e64104788c53092a
```

The matching plug-in source snapshot is:

```text
xweather_routing_pi-f6891f8-source.tar.gz
size: 4.2 MiB
sha256: afaa3cd4376dc3029a5f753343badebd5c3ec211a9bee52a4928430e323a992f
```

The binary archive contains `bin/opencpn`, standard bundled plug-ins, the exact
`libxweather_routing_pi.so`, plug-in translations, boat/polar data and normal
OpenCPN shared data. Its launcher sets separate library/data search paths and
uses an isolated profile. This matters because arbitrary third-party plug-ins
are deliberately not loaded from OpenCPN's system plug-in directory.

The archive was compiled for `/tmp/opencpn-hardening-install` and must be
extracted with `tar -C /tmp`; it is not generally relocatable. An extracted-tree
smoke test loaded the packaged weather-routing plug-in, registered its immutable
tile cache, reported full chart-aware safety, completed real CM93 land/depth and
final-route cases with zero failures, unregistered the cache and exited zero.
The package listed above predates the v3 CM93 seam correction and plug-in commit
`b411e62`; it is superseded and must not be distributed as the current candidate.
The replacement v3 artifact and matching source snapshot are recorded below.

```text
OpenCPN-5.15.0-core-hardening-v3-chart-aware-debug-arch-x86_64.tar.gz
size: 135 MiB (141,380,597 bytes)
sha256: bbdb28d09691685846b8ba67ab2a81f4f2f15a8fcbe65814d50b6688b6a0665e

xweather_routing_pi-b411e62-source.tar.gz
size: 4.2 MiB (4,355,265 bytes)
sha256: 8ea933413a57050eef98edf78cb8ca52ca26517d3d06c20946f23fa89043272c
```

The v3 bundle contains core integration commit
`f87ee79311e62b09983c5006fe474012cf07da3a` and the weather-routing binary
from plug-in commit `b411e62ed8059549925b2ec66f986cb1ca586db9`. Its unpacked core executable
has SHA-256 `641d812fe0ca7ba07717c28166faf09cb3761d07c73b094d7e3b9ff55b8469fb`;
the preferred `extra-plugins/libxweather_routing_pi.so` has SHA-256
`7129556755894f860aa5cd076e9491da595f1bb44411d9f78e1b4544241f0576`.

It was created on the audit Arch Linux x86_64 host and is not a portable,
signed or official distribution. Prefer rebuilding from the manifest on a
matching target. It must not replace a working vessel installation or be used
as the sole means of navigation.

This is one Linux build result, not the cross-platform acceptance matrix.
Windows, macOS, Android/ARM and physical N2K/autopilot testing remain explicit
release gates in the implementation plan.
