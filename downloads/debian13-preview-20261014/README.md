# Debian 13 x86_64 — Chart-Aware OpenCPN Preview

Native installer for Debian 13 (trixie) **amd64 / Intel or AMD 64-bit laptops and PCs**.
Built from the exact core used by the [12 October developer bundle](https://github.com/pob220/OpenCPN-Core-Hardening/releases/tag/chart-aware-routing-preview-20260831):
[`23c95ad379b8`](https://github.com/pob220/OpenCPN-Core-Hardening/tree/23c95ad379b83dd7a97e2edd7d6a859d49099a91).
Includes xWeatherRouting 1.30.0, xGRIB, Climatology 2026.2, Polar, Celestial
Navigation 2.9.8, modified o-charts, Scheduler, Python SDK and MCP tools.

## Downloads

| File | Public download |
|---|---|
| Debian 13 amd64 installer (141 MiB) | [Download .deb](https://github.com/pob220/OpenCPN-Core-Hardening/releases/download/debian13-chart-aware-preview-20261014/opencpn-chart-aware-preview_5.15.0+chartaware.20261014.1+deb13_amd64.deb) |
| Installer checksum | [Download .sha256](https://github.com/pob220/OpenCPN-Core-Hardening/releases/download/debian13-chart-aware-preview-20261014/opencpn-chart-aware-preview_5.15.0+chartaware.20261014.1+deb13_amd64.deb.sha256) |
| Installation guide | [Download INSTALL.md](https://github.com/pob220/OpenCPN-Core-Hardening/releases/download/debian13-chart-aware-preview-20261014/INSTALL.md) |
| Build and validation report | [Download BUILD-REPORT.md](https://github.com/pob220/OpenCPN-Core-Hardening/releases/download/debian13-chart-aware-preview-20261014/BUILD-REPORT.md) |
| Component revisions | [Download components.json](https://github.com/pob220/OpenCPN-Core-Hardening/releases/download/debian13-chart-aware-preview-20261014/components.json) |
| All-file checksums | [Download SHA256SUMS](https://github.com/pob220/OpenCPN-Core-Hardening/releases/download/debian13-chart-aware-preview-20261014/SHA256SUMS) |
| Complete matching core source | [Download source archive](https://github.com/pob220/OpenCPN-Core-Hardening/releases/download/debian13-chart-aware-preview-20261014/opencpn-core-23c95ad379b8-source.tar.gz) |
| Debian packaging source | [Download packaging archive](https://github.com/pob220/OpenCPN-Core-Hardening/releases/download/debian13-chart-aware-preview-20261014/debian13-packaging-source.tar.gz) |
| Clean Debian 13 qualification log | [Download qualification log](https://github.com/pob220/OpenCPN-Core-Hardening/releases/download/debian13-chart-aware-preview-20261014/qualification-debian13-amd64.log) |
| Automated test results | [Download test-results.json](https://github.com/pob220/OpenCPN-Core-Hardening/releases/download/debian13-chart-aware-preview-20261014/test-results.json) |

All files are grouped on the [Debian 13 release page](https://github.com/pob220/OpenCPN-Core-Hardening/releases/tag/debian13-chart-aware-preview-20261014).
Large binaries are stored as release assets so they can be downloaded directly.

## Install

Download the installer and its checksum into the same directory, then run:

```sh
sha256sum --check opencpn-chart-aware-preview_5.15.0+chartaware.20261014.1+deb13_amd64.deb.sha256
sudo apt install ./opencpn-chart-aware-preview_5.15.0+chartaware.20261014.1+deb13_amd64.deb
opencpn-chart-aware
```

Or launch **OpenCPN Chart-Aware Preview** from the application menu. On first
run choose a fresh profile or import settings into a separate working copy.
The ordinary OpenCPN installation and profile remain separate.

Core/plugin tests and clean Debian 13 installation, software/OpenGL startup,
API/SDK/Scheduler/MCP, weather helper, reinstall and removal checks passed.
See [BUILD-REPORT.md](BUILD-REPORT.md) for evidence and qualification limits.

This is a developer preview. Supply charts licensed to the test machine and
forecasts for native chart-aware routing tests. The package includes no licensed
charts, entitlements, forecasts, chart caches or large optional celestial packs.
