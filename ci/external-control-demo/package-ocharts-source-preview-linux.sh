#!/usr/bin/env bash
set -euo pipefail

if (( $# != 5 )); then
  echo "usage: package-ocharts-source-preview-linux.sh SOURCE-DIRECTORY WORK-DIRECTORY OFFICIAL-ARCHIVE OUTPUT-ARCHIVE EXPECTED-ELF-MACHINE" >&2
  exit 2
fi

source_dir=$(realpath "$1")
work_dir=$(realpath -m "$2")
official_archive=$(realpath "$3")
output_archive=$(realpath -m "$4")
expected_machine=$5
build_dir="$work_dir/build"

rm -rf "$work_dir"
mkdir -p "$build_dir" "$(dirname "$output_archive")"

# Configure the actual plug-in tree directly.  The template's top-level
# `tarball` target reconfigures this same directory and is not idempotent with
# all CMake versions; the official package supplies the licensed helper and
# runtime payload, so only the provider library needs to be rebuilt here.
cmake -S "$source_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$work_dir/install" \
  -DBUILD_TYPE=tarball
cmake --build "$build_dir" --target o-charts_pi \
  --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"

plugin="$build_dir/libo-charts_pi.so"
test -f "$plugin"
machine=$(readelf -h "$plugin" | sed -n 's/^ *Machine: *//p')
[[ $machine == "$expected_machine" ]]
readelf -Ws "$plugin" | grep -F 'OCPN_PluginChartSafetyGridV1' >/dev/null

"$(dirname "$0")/package-ocharts-preview-linux.sh" \
  "$official_archive" "$plugin" "$output_archive" "$expected_machine"
