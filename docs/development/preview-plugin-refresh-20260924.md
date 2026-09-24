# 24 September 2026 — refreshed plugin bundle

The Debian 12 x86_64 and ARM64 previews pin xWeatherRouting 1.18.1.0 at
`17982619b73c7fc265a03c7891ff0534795bc471` and Celestial Navigation 2.9.0.0
alpha at `67bd8fffc8d5e6dbad246bbac88832f8655bbe2c`. xGRIB 0.3.0.0,
Generator 0.3.0 and the other component pins are retained from the
21 September assembly.

The xWeatherRouting change broadens Quick's bounded tack/gybe connector
search. It retains the existing physical constraints and chronological
validation. Celestial includes the 2.9 planner, sight visibility and display
work, the 2.8.11 baseline, and the older-wxWidgets compatibility correction.
Optional astronomy kernels and lunar-terrain data remain opt-in.

## Plugin import isolation

The previous launcher searched only its bundled library directory, while
Plugin Manager installed imports under `~/.local`. Adding that entire user
directory to the search path also exposed unrelated native plugins to a
Debian 12 container. Installation records and failed-load markers could
additionally come from the user's ordinary profile despite `--configdir`.

Native Linux now accepts `OPENCPN_PLUGIN_INSTALL_PREFIX` for Plugin Manager's
library, helper and data destinations. The preview sets it to its own
user-writable `usr/local` directory, matching `OPENCPN_PLUGIN_DIRS`. Imports
replace the bundled copy without introducing a second fallback copy.
Plugin data lookup excludes the implicit `~/.local/share` directory in this
mode. Installation records and load stamps use the active private profile.

The launcher and README continue to require a fresh installation. Existing
preview launchers are not modified. Native Linux installations that do not
set the override retain their normal `~/.local` installation prefix.

## Qualification gates

- Rebuild core and plugins on both Debian 12 architectures from pinned source.
- Core tests cover private data lookup, profile-specific installation records
  and failed-load markers alongside the existing API and chart-safety tests.
- Plugin package tests and eight isolated Celestial GUI suites must pass.
- Clean-install qualification imports the real xWeatherRouting archive twice,
  verifies its binary checksum and 1.18.1.0 installation metadata, and confirms
  all imported regular files stay inside the preview prefix.
- A subsequent GUI/API smoke run must load the imported routing library,
  retain the environmental/planning providers and exclude ordinary user
  plugin library/data paths.
- Release archives receive external and internal checksum verification before
  publication. Qualification logs accompany both downloads.
