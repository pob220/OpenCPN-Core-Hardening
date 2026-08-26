#!/usr/bin/env bash
set -euo pipefail

if (( $# != 10 )); then
  cat >&2 <<'EOF'
usage: assemble-linux.sh CORE_INSTALLER XGRIB_ARCHIVE XWEATHER_ARCHIVE \
  CLIMATOLOGY_ARCHIVE POLAR_ARCHIVE CELESTIAL_ARCHIVE SCHEDULER_REPOSITORY \
  OUTPUT_DIRECTORY PLATFORM EXPECTED_UNAME

Example PLATFORM: debian12-arm64
Example EXPECTED_UNAME: aarch64
EOF
  exit 2
fi

core_installer=$(realpath "$1")
xgrib_archive=$(realpath "$2")
xweather_archive=$(realpath "$3")
climatology_archive=$(realpath "$4")
polar_archive=$(realpath "$5")
celestial_archive=$(realpath "$6")
scheduler_repo=$(realpath "$7")
output_dir=$(realpath -m "$8")
platform=$9
expected_uname=${10}
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_root=$(git -C "$script_dir/../.." rev-parse --show-toplevel)
assembly_revision=$(git -C "$source_root" rev-parse HEAD)
core_revision=${EXTERNAL_CONTROL_CORE_REVISION:-$assembly_revision}
release_date=${EXTERNAL_CONTROL_DEMO_DATE:-$(date -u +%Y-%m-%d)}
bundle_name="OpenCPN-External-Control-Demo-${release_date}-${platform}"
final_bundle_dir="$output_dir/$bundle_name"
archive="$output_dir/$bundle_name.tar.gz"
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/external-control-demo.XXXXXX")
bundle_dir="$work_dir/$bundle_name"
trap 'rm -rf "$work_dir"' EXIT

for path in "$core_installer" "$xgrib_archive" "$xweather_archive" \
  "$climatology_archive" "$polar_archive" "$celestial_archive"; do
  test -f "$path" || { echo "Missing input: $path" >&2; exit 1; }
done
for path in \
  "$source_root/external/opencpn-control-python/pyproject.toml" \
  "$source_root/external/opencpn-mcp/pyproject.toml" \
  "$scheduler_repo/pyproject.toml"; do
  test -f "$path" || { echo "Missing Python source: $path" >&2; exit 1; }
done
if [[ -e "$final_bundle_dir" || -e "$archive" ]]; then
  echo "Refusing to overwrite existing output for $bundle_name" >&2
  exit 1
fi

mkdir -p "$bundle_dir/assets" "$bundle_dir/docs" "$bundle_dir/launchers" \
  "$work_dir/python"
install -m 755 "$core_installer" \
  "$bundle_dir/assets/opencpn-core-external-control-demo-${platform}-installer.sh"
install -m 644 "$xgrib_archive" \
  "$bundle_dir/assets/xgrib-external-control-demo-${platform}.tar.gz"
install -m 644 "$xweather_archive" \
  "$bundle_dir/assets/xweather-routing-external-control-demo-${platform}.tar.gz"
install -m 644 "$climatology_archive" \
  "$bundle_dir/assets/climatology-external-control-demo-${platform}.tar.gz"
install -m 644 "$polar_archive" \
  "$bundle_dir/assets/polar-external-control-demo-${platform}.tar.gz"
install -m 644 "$celestial_archive" \
  "$bundle_dir/assets/celestial-navigation-external-control-demo-${platform}.tar.gz"

project_number=0
for project in \
  "$source_root/external/opencpn-control-python" \
  "$source_root/external/opencpn-mcp" \
  "$scheduler_repo"; do
  project_number=$((project_number + 1))
  project_copy="$work_dir/python-source-$project_number"
  cp -a "$project/." "$project_copy/"
  rm -rf "$project_copy/.git" "$project_copy/build"
  find "$project_copy" -type d -name '*.egg-info' -prune -exec rm -rf '{}' +
  python3 -m build --no-isolation --wheel --sdist \
    --outdir "$work_dir/python" "$project_copy"
done
install -m 644 "$work_dir/python"/* "$bundle_dir/assets/"

install -m 644 "$source_root/api/openapi-v2.yaml" "$bundle_dir/docs/"
install -m 644 "$source_root/api/scheduler-schedule-v1.schema.json" \
  "$bundle_dir/docs/"
install -m 644 \
  "$source_root/docs/development/scheduler-preview-c-contract.md" \
  "$bundle_dir/docs/scheduler-contract.md"

sed \
  -e "s/@PLATFORM@/$platform/g" \
  -e "s/@EXPECTED_UNAME@/$expected_uname/g" \
  "$source_root/tools/external-control-demo/install-linux.in" \
  > "$bundle_dir/install-external-control-demo.sh"
sed \
  -e "s/@PLATFORM@/$platform/g" \
  -e "s/@EXPECTED_UNAME@/$expected_uname/g" \
  "$source_root/tools/external-control-demo/launch-opencpn-linux.in" \
  > "$bundle_dir/launchers/launch-opencpn-external-control-demo"
install -m 755 \
  "$source_root/tools/external-control-demo/launch-scheduler-linux" \
  "$bundle_dir/launchers/launch-opencpn-scheduler-external-control-demo"
install -m 755 \
  "$source_root/tools/external-control-demo/launch-mcp-linux" \
  "$bundle_dir/launchers/launch-opencpn-mcp-external-control-demo"
chmod 755 "$bundle_dir/install-external-control-demo.sh" \
  "$bundle_dir/launchers/launch-opencpn-external-control-demo"

sed \
  -e "s/@PLATFORM@/$platform/g" \
  -e "s/@EXPECTED_UNAME@/$expected_uname/g" \
  "$source_root/tools/external-control-demo/README-linux.in" \
  > "$bundle_dir/README.md"

cat > "$bundle_dir/COMPONENTS.md" <<EOF
# Component manifest

| Component | Revision |
|---|---|
| OpenCPN hardened core binary and API | $core_revision (external-control code based on release tag external-control-demo-20260821) |
| Demo assembly and qualification tooling | $assembly_revision |
| xGRIB provider build | 369b6fc8a4c461d54eec75d949e1f2e30cccdc48 (provider behavior based on 6674c70583d285cdcbc622f3377da810fde0d3ba) |
| xWeatherRouting provider implementation | 5dc04608945e1917e8cbb4e8df274619c1b5203d |
| Upgraded Climatology plug-in and dataset 2026.2 | cd00282e6ea2784a6d78ccfe47fed713269ad87e |
| Polar plug-in | 1.2.38.0 official OpenCPN package, SHA-256 $(sha256sum "$polar_archive" | cut -d ' ' -f 1) |
| Upgraded Celestial Navigation plug-in | 2.7.0.0 at 316ed8bc326b29c6d2c358835164df995a8e06af |
| OpenCPN Scheduler | $(git -C "$scheduler_repo" rev-parse HEAD) |

Target: \`$platform\` (kernel architecture \`$expected_uname\`). Native plug-in
API remains 1.21. The Python SDK, MCP adapter and Scheduler are built from the
listed source trees during assembly.
EOF

cat > "$bundle_dir/FULL-SYSTEM-CHECKLIST.md" <<'EOF'
# Qualified full-system surface

The bundle qualification fails unless all of these surfaces are present and
usable in a clean, isolated installation:

- hardened OpenCPN core and authenticated `/api/v2` service;
- nine native plug-in binaries and their resource directories;
- enabled xGRIB environmental-data and xWeatherRouting planning providers;
- upgraded Climatology, Polar and Celestial Navigation plug-ins;
- versioned OpenAPI and Scheduler JSON contracts;
- Python SDK and `opencpnctl` CLI;
- least-privilege MCP adapter and deterministic MCP protocol canary;
- Scheduler service and GUI;
- loopback-only generated credentials with no route-activation, autopilot or
  raw-message authority;
- architecture, shared-library, checksum, lifecycle, API capability and
  clean-install checks.

Charts, forecast downloads, polar choices and the large optional Celestial
eclipse kernels remain user-supplied. Their absence is deliberate and is not
reported as a qualified capability.
EOF

cat > "$bundle_dir/PLUGIN-INVENTORY.md" <<'EOF'
# Included native plug-ins

These plug-ins are installed directly in the isolated demo. They do not depend
on an entry in the public OpenCPN Master catalogue.

| Plug-in | Demo role | Initial state |
|---|---|---|
| Chart Downloader | Bundled OpenCPN chart acquisition | Available, disabled |
| Dashboard | Bundled navigation instruments | Available, disabled |
| GRIB | Bundled compatibility plug-in | Available, disabled in favour of xGRIB |
| WMM | Bundled magnetic variation | Available, disabled |
| xGRIB | Typed environmental-data provider and GRIB display | Enabled |
| xWeatherRouting | Typed chart/weather route-planning provider | Enabled |
| Climatology 1.6.39 / dataset 2026.2 | Updated offline climate statistics | Enabled |
| Polar 1.2.38 | Polar inspection and editing companion | Enabled |
| Celestial Navigation 2.7 | Offline almanac, sight planning and fixes | Enabled |

The installer and CI qualification both fail if any listed binary is absent.
EOF

cat > "$bundle_dir/CELESTIAL-ECLIPSE-DATA.md" <<'EOF'
# Optional Celestial Navigation eclipse data

The upgraded Celestial Navigation plug-in is included, but the large optional
eclipse kernels and lunar-terrain packs are deliberately not embedded in this
External Control Demo. They are not required for the almanac, sight-planning
or celestial-fix features demonstrated by the baseline.

Developers who want to source-build and test the complete offline eclipse
feature can use the exact pinned implementation and instructions:

- [Pinned Celestial Navigation source](https://github.com/pob220/celestial_navigation_pi-contrib/tree/316ed8bc326b29c6d2c358835164df995a8e06af)
- [Eclipse engine and build files](https://github.com/pob220/celestial_navigation_pi-contrib/tree/316ed8bc326b29c6d2c358835164df995a8e06af/eclipse)
- [Exact data files, checksums and provenance](https://github.com/pob220/celestial_navigation_pi-contrib/blob/316ed8bc326b29c6d2c358835164df995a8e06af/eclipse/DATA.md)
- [Unsplit optional data release](https://github.com/pob220/celestial_navigation_pi/releases/tag/eclipse-data-2026.1)

The repository stores the data files with Git LFS. A Git-LFS-enabled checkout
or the separate release assets supplies them. The Celestial code validates the
documented sizes, SHA-256 digests and kernel structures before accepting them;
the demo does not download or silently install any kernel.
EOF

(cd "$bundle_dir" && find assets -type f -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS)
mkdir -p "$output_dir"
mv "$bundle_dir" "$final_bundle_dir"
tar -C "$output_dir" -czf "$archive" "$bundle_name"
(cd "$output_dir" && sha256sum "$bundle_name.tar.gz" > "$bundle_name.tar.gz.sha256")
printf 'Created %s\n' "$archive"
