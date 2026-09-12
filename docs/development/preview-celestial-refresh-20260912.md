# Celestial Navigation preview refresh — 12 September 2026

The existing Core-Hardening preview contains Celestial Navigation 2.8.5.3.
This refresh pins 2.8.5.4 at `a003cf7d36880ca38d15a172bcde48ef5c3fa421`
for both Debian 12 x86_64 and ARM64 bundles.

The new version separates Find position edits from explicit estimated-Hs copying,
adds independent reset/cancel behavior and observed-altitude display, clarifies
the time-panel toggle, and ranks ambiguous lunar UTC/position solutions using
valid DR proximity while preserving explicit selection.

The package qualification now runs the Find, lunar and coastal GUI regression
suites individually under Xvfb, alongside the ordinary CTest targets. Existing
bundle installation and API/GUI smoke qualification remains required.

Weather Routing 1.17.4.0, xGRIB 0.2.5.2, Generator 0.1.8 and the remaining component
pins are retained. The existing release URL and 12 September date are retained.
Final results and checksums will be recorded after qualification.
