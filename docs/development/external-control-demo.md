# OpenCPN External Control Demo

The External Control Demo is the single developer entry point for the
authenticated `/api/v2` work. It combines the hardened OpenCPN 5.15 core,
xGRIB, xWeatherRouting and the independent Scheduler without merging their
source repositories or changing native plug-in API 1.21.

Release and one-package installation:

<https://github.com/pob220/OpenCPN-Core-Hardening/releases/tag/external-control-demo-20260821>

The original release archive contains exact component revisions, checksums,
offline Python wheels, resource-complete plug-ins, generic launchers, wire
schemas and an isolated installer. The installer refuses to overwrite a
non-empty directory, generates a least-privilege local token, uses a separate
OpenCPN configuration and does not require `sudo`.

The demonstration exercises two useful vertical slices:

1. scheduled xGRIB environmental acquisition followed by activation in the
   running OpenCPN display; and
2. acquisition followed by xWeatherRouting using the exact immutable dataset,
   with independent core chart/land/depth validation before publishing a
   non-active draft.

It is a developer demonstration, not a navigation release. The release page
and each bundled qualification document state the tested platform, evidence
and limitations. Existing onboard profiles and installations must not be used
as the test target.

## Cross-platform rebuild

The `external-control/cross-platform-demo` branch makes the demonstration
reproducible rather than treating the original Arch Linux x86_64 archive as a
portable binary. The first complete additional target is Debian 12 ARM64 for
Raspberry Pi 4 and newer 64-bit systems. Native component builds for Windows,
macOS and other Linux distributions are characterised by the same programme;
they are not described as complete demo packages until their platform-specific
installer and end-to-end qualification also pass.

Linux build and release tooling lives in `ci/external-control-demo` and
`tools/external-control-demo`. It provides:

- a native build, focused external-control tests and retained logs;
- an audit of every executable and shared library's ELF architecture;
- deterministic assembly from exact core, xGRIB, xWeatherRouting and Scheduler
  revisions;
- CPU checks before extraction and again before launch;
- an atomic isolated install with shared-library diagnostics;
- checksum, owner-only secret, least-privilege scope and overwrite tests; and
- an optional headless API-readiness smoke test on the native target.

The Linux package deliberately uses normal target-distribution shared
libraries while keeping all OpenCPN state, plug-ins, API credentials and
Python tooling below its selected test directory. It neither installs over a
system OpenCPN nor reads a live sailing profile.
