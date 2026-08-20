# External-control Preview B.1 hardening qualification

Date: 20 August 2026

Preview B.1 is a code integration, not a package-only update. It starts from
the published Preview B core and applies the complete issue-led hardening
series, including the numeric WMM message correction. The earlier Preview B
chart-depth and time-budget implementation was already identical to the
core-hardening implementation and was retained once, with its tests.

## Tested revisions

- OpenCPN: release tag `external-control-preview-b1-hardening-20260820` on
  branch `external-control/preview-b.1-hardening`;
- xWeatherRouting: `3ec2e6454f8eb9a366ed304f5f5fff176deeb231` on
  `external-control/preview-b`;
- xGRIB: `5c9b4976812f015a7cd5e5706a878447c2f2e9c6` on `main`.

The release `SHA256SUMS` is the authority for every binary and archive.

## Automated evidence

- combined OpenCPN RelWithDebInfo application and test targets built;
- OpenCPN deterministic suite: 120/120 passed sequentially;
- complete GCC AddressSanitizer deterministic suite: 120/120 passed with
  `abort_on_error=1`;
- xWeatherRouting stock-host suite: 176/176 passed;
- xWeatherRouting hardened-host shared library built against this combined
  core source;
- xGRIB suite: 22/22 passed;
- Python SDK: 3/3 passed;
- MCP protocol suite: 7/7 passed.
- the self-extracting installer unpacked 790 core payload files into an
  unprivileged isolated root; the freshly built xGRIB and hardened
  xWeatherRouting archives overlaid successfully and all three ELF runtime
  files were present with the expected architecture.

The first extracted-runtime attempt also caught an inherited packaging error:
`--prefix` relocated the CPack files but wxWidgets still used the compiled
`/usr/local` resource prefix. `OPENCPN_PREFIX` now configures shared-data and
plug-in paths consistently, has a deterministic regression test, and is set by
the supplied launcher. Runtime qualification was restarted from the rebuilt
package; the failed pre-fix attempt is not counted as a pass.

The rebuilt installed-package gate subsequently caught two additional package
boundary defects. Relocated plug-in data now takes precedence over unrelated
user/system data, and xGRIB/xWeatherRouting are admitted by exact plug-in name
instead of requiring the broad arbitrary-system-plug-in switch. The local
control listener now also starts with a loopback certificate when no external
network interface exists; mDNS remains conditional on a real interface.

The final fresh installed run used a private network namespace containing only
loopback. It loaded xGRIB and xWeatherRouting code and data from the extracted
root, registered `route-planning.chart-weather.v1`, created the development
certificate, and passed authenticated version, capabilities and readiness
requests. The packaged Python CLI passed status, navigation and route-list
queries, and the WebSocket client received the bounded initial snapshot and
subscription acknowledgement. Neither running desktop OpenCPN instance was
stopped or used by this gate.

LeakSanitizer is disabled for the sanitizer run because the managed ptrace
environment makes LSan itself terminate. The historical aggregate test and
desktop-session IPC registrations are not counted as deterministic passes.

## Behavioural evidence retained from the merged lines

The hardening branch already passed the isolated Celestial Navigation/WMM GUI
gate and the real CM93 chart-aware Holyhead-to-Foyle acceptance at an explicit
5 m minimum depth. Preview B already passed authenticated HTTPS, WebSocket,
CLI and MCP workflows, transactional draft persistence, chart-direct and
resident xWeatherRouting planning, fail-closed land rejection, cancellation
and immediate provider reuse. Preview B.1 changes neither wire contract nor
provider ABI; its focused and deterministic reruns cover the merged boundary.

## Limits

This is still a Linux x86_64 developer preview, not an official navigation
release. Hardware N2K/Actisense, real autopilot families, licensed-chart SENC
shutdown, macOS texture pressure, Windows/macOS packaging and cross-platform
plug-in unload require their respective test matrices. The supplied plug-ins
remain separate projects and are release assets rather than vendored core
source. Completed external plans remain drafts and cannot be activated without
the separately scoped, explicitly confirmed command.
