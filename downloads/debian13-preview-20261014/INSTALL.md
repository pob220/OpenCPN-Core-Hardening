# Debian 13 developer-preview installer

This amd64 package rebuilds the exact core and open-source plugin revisions from
the **12 October 2026** developer bundle on Debian 13 (trixie). The core is
`23c95ad379b83dd7a97e2edd7d6a859d49099a91`, with no source-code modifications.
It retains the external-control API, hardened core, chart/depth service and
geometry proofs used by that bundle. It is experimental testing software.

## Install and start

In the directory containing the package and matching checksum:

```sh
sha256sum --check opencpn-chart-aware-preview_5.15.0+chartaware.20261014.1+deb13_amd64.deb.sha256
sudo apt install ./opencpn-chart-aware-preview_5.15.0+chartaware.20261014.1+deb13_amd64.deb
opencpn-chart-aware
```

APT installs the required Debian 13 runtime libraries. Launch **OpenCPN
Chart-Aware Preview** from the application menu as your ordinary desktop user.
The package runs natively; Docker and a development toolchain are not needed.

Close existing OpenCPN instances before first-run setup. Choose a fresh profile
or import settings from an existing native/Flatpak profile. Import makes a
verified backup and separate working copy; chart paths continue to refer to your
existing chart files. Imported binary plugins and rebuildable caches are excluded.
Imported input/output connections start disabled; review them in Options.

Program files: `/usr/lib/opencpn-chart-aware/`.
Preview profile: `~/.local/share/opencpn-chart-aware/profile/`.
Settings backups: `~/.local/share/opencpn-chart-aware/backup-*/`.
The ordinary `opencpn` command and profile remain separate.

## Included features

- Published hardened core and chart-safety geometry, rebuilt on Debian 13.
- xWeatherRouting **1.30.0**, native plugin API **1.21**.
- xGRIB **0.3.9.0**, Environmental GRIB Generator **0.3.4**.
- Climatology **2026.2**, modified o-charts provider, Polar **1.2.38.0**.
- Celestial Navigation **2.9.8.0**, with compact ephemerides.
- Scheduler **0.2.0**, Python SDK **0.2.0**, MCP adapter **0.1.0**.
- Standard Chart Downloader, Dashboard, WMM and GRIB plugins. Native GRIB is
  initially disabled to keep xGRIB as the active weather provider.

The official Polar binary and licensed o-charts helper/runtime are preserved from
the pinned vendor packages and checked on Debian 13. Core, xWeatherRouting, xGRIB,
Climatology, Celestial and the modified o-charts provider are built on Debian 13.
No licensed charts, forecasts, entitlements or semantic caches are included.
Large optional celestial eclipse/lunar-terrain packs remain separate.

Load your charts and a forecast, configure a boat/polar and use xWeatherRouting's
chart/depth-aware options. For o-charts, use charts licensed to this machine and
the normal helper/dongle/TPM setup. Displaying a licensed chart and testing an
accepted and rejected route is needed to establish chart-provider access.

## Additional preview tools

After first-run setup, start OpenCPN and use:

```sh
opencpn-chart-aware-scheduler
opencpn-chart-aware-ctl status
opencpn-chart-aware-mcp
```

The API listens on loopback `https://127.0.0.1:8443`, with a private, newly generated
token in `profile/secrets/api-token`. The tools use that token automatically.
Avoid running another preview on the same port. MCP uses stdin/stdout transport.
Scheduler defaults to its GUI and keeps its database inside the preview profile.

## Updates and recovery

Close OpenCPN before upgrading with APT. Existing preview settings are retained.
`opencpn-chart-aware --software` backs up its configuration and switches to
software rendering. OpenGL is available in first-run setup and Options.

```sh
opencpn-chart-aware --diagnostics
sudo apt remove opencpn-chart-aware-preview
```

Removal leaves personal profiles, backups and chart files intact. Diagnostics
print component pins and installed plugin names without printing credentials.

The build report distinguishes automated geometry/plugin tests and clean Debian
GUI/API smoke from real licensed-chart and physical GPU/hardware testing.
