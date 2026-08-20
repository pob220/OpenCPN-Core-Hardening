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
- OpenCPN deterministic suite: 119/119 passed sequentially;
- focused GCC AddressSanitizer safety/API suite: 51/51 passed with
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

LeakSanitizer is disabled for the focused run because the managed ptrace
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
