#!/usr/bin/env bash
set -euo pipefail

if (( $# != 4 )); then
  echo "usage: package-celestial-linux.sh SOURCE-DIRECTORY WORK-DIRECTORY OUTPUT-ARCHIVE EXPECTED-ELF-MACHINE" >&2
  exit 2
fi

source_dir=$(realpath "$1")
work_dir=$(realpath -m "$2")
output_archive=$(realpath -m "$3")
expected_machine=$4
build_dir="$work_dir/build"
stage_dir="$work_dir/stage"

rm -rf "$work_dir"
mkdir -p "$build_dir" "$stage_dir" "$(dirname "$output_archive")"

cmake -S "$source_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DOCPN_BUILD_TEST=ON
cmake --build "$build_dir" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
ctest --test-dir "$build_dir" --output-on-failure
DESTDIR="$stage_dir" cmake --install "$build_dir"

plugin="$stage_dir/usr/local/lib/opencpn/libcelestial_navigation_pi.so"
data_dir="$stage_dir/usr/local/share/opencpn/plugins/celestial_navigation_pi/data"
test -f "$plugin"
test -f "$data_dir/vsop87d.txt"
test -f "$data_dir/Celestial_Navigation_Information.html"
machine=$(readelf -h "$plugin" | sed -n 's/^ *Machine: *//p')
[[ $machine == "$expected_machine" ]]

# Eclipse kernels are optional source-build inputs, not part of the demo.
if find "$stage_dir" -type f \
    \( -name '*.bsp' -o -name '*.bpc' -o -name 'lola64-pa.bin' \) \
    -print -quit | grep -q .; then
  echo "Celestial package unexpectedly contains an eclipse kernel" >&2
  exit 1
fi

tar -C "$stage_dir" -czf "$output_archive" usr/local
printf 'Created qualified Celestial Navigation package: %s\n' "$output_archive"
