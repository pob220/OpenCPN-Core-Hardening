#!/usr/bin/env bash
set -euo pipefail

if (( $# != 7 )); then
  cat >&2 <<'EOF'
usage: assemble-linux.sh CORE_INSTALLER XGRIB_ARCHIVE XWEATHER_ARCHIVE \
  SCHEDULER_REPOSITORY OUTPUT_DIRECTORY PLATFORM EXPECTED_UNAME

Example PLATFORM: debian12-arm64
Example EXPECTED_UNAME: aarch64
EOF
  exit 2
fi

core_installer=$(realpath "$1")
xgrib_archive=$(realpath "$2")
xweather_archive=$(realpath "$3")
scheduler_repo=$(realpath "$4")
output_dir=$(realpath -m "$5")
platform=$6
expected_uname=$7
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
source_root=$(git -C "$script_dir/../.." rev-parse --show-toplevel)
release_date=${EXTERNAL_CONTROL_DEMO_DATE:-$(date -u +%Y-%m-%d)}
bundle_name="OpenCPN-External-Control-Demo-${release_date}-${platform}"
final_bundle_dir="$output_dir/$bundle_name"
archive="$output_dir/$bundle_name.tar.gz"
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/external-control-demo.XXXXXX")
bundle_dir="$work_dir/$bundle_name"
trap 'rm -rf "$work_dir"' EXIT

for path in "$core_installer" "$xgrib_archive" "$xweather_archive"; do
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
| OpenCPN hardened core and API | $(git -C "$source_root" rev-parse HEAD) (external-control code based on release tag external-control-demo-20260821) |
| xGRIB provider build | 369b6fc8a4c461d54eec75d949e1f2e30cccdc48 (provider behavior based on 6674c70583d285cdcbc622f3377da810fde0d3ba) |
| xWeatherRouting provider implementation | 5dc04608945e1917e8cbb4e8df274619c1b5203d |
| OpenCPN Scheduler | $(git -C "$scheduler_repo" rev-parse HEAD) |

Target: \`$platform\` (kernel architecture \`$expected_uname\`). Native plug-in
API remains 1.21. The Python SDK, MCP adapter and Scheduler are built from the
listed source trees during assembly.
EOF

(cd "$bundle_dir" && find assets -type f -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS)
mkdir -p "$output_dir"
mv "$bundle_dir" "$final_bundle_dir"
tar -C "$output_dir" -czf "$archive" "$bundle_name"
(cd "$output_dir" && sha256sum "$bundle_name.tar.gz" > "$bundle_name.tar.gz.sha256")
printf 'Created %s\n' "$archive"
