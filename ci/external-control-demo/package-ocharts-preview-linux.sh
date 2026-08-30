#!/usr/bin/env bash
set -euo pipefail

if (( $# < 4 || $# > 5 )); then
  echo "usage: package-ocharts-preview-linux.sh OFFICIAL-ARCHIVE MODIFIED-PLUGIN OUTPUT-ARCHIVE EXPECTED-ELF-MACHINE [RUNTIME-LIB-DIRECTORY]" >&2
  exit 2
fi

official_archive=$(realpath "$1")
modified_plugin=$(realpath "$2")
output_archive=$(realpath -m "$3")
expected_machine=$4
runtime_lib_dir=${5:-}
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/ocharts-preview-package.XXXXXX")
trap 'rm -rf "$work_dir"' EXIT

test -f "$official_archive"
test -f "$modified_plugin"
mkdir -p "$work_dir/unpacked" "$(dirname "$output_archive")"
tar -xzf "$official_archive" -C "$work_dir/unpacked"

mapfile -t package_roots < <(find "$work_dir/unpacked" -mindepth 1 -maxdepth 1 -type d -print)
if (( ${#package_roots[@]} != 1 )); then
  echo "Official o-charts archive does not contain exactly one package root" >&2
  exit 1
fi
package_root=${package_roots[0]}
plugin="$package_root/lib/opencpn/libo-charts_pi.so"
helper="$package_root/bin/oexserverd"
test -f "$plugin"
test -x "$helper"
install -m 755 "$modified_plugin" "$plugin"
if [[ -n $runtime_lib_dir ]]; then
  runtime_lib_dir=$(realpath "$runtime_lib_dir")
  test -d "$runtime_lib_dir"
  cp -a "$runtime_lib_dir"/. "$package_root/lib/opencpn/"
fi

machine=$(readelf -h "$plugin" | sed -n 's/^ *Machine: *//p')
[[ $machine == "$expected_machine" ]]
readelf -Ws "$plugin" | grep -F 'OCPN_PluginChartSafetyGridV1' >/dev/null

tar -C "$work_dir/unpacked" -czf "$output_archive" "$(basename "$package_root")"
printf 'Created o-charts chart-safety preview package: %s\n' "$output_archive"
