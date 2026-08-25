#!/usr/bin/env bash
set -euo pipefail

if (( $# != 4 )); then
  echo "usage: package-climatology-linux.sh SOURCE-DIRECTORY WORK-DIRECTORY OUTPUT-ARCHIVE EXPECTED-ELF-MACHINE" >&2
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
  -DBUILD_TESTING=ON
cmake --build "$build_dir" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
ctest --test-dir "$build_dir" --output-on-failure
DESTDIR="$stage_dir" cmake --install "$build_dir"

plugin="$stage_dir/usr/local/lib/opencpn/libclimatology_pi.so"
data_dir="$stage_dir/usr/local/share/opencpn/plugins/climatology_pi/data"
manifest="$data_dir/dataset-manifest.json"
test -f "$plugin"
test -f "$manifest"
machine=$(readelf -h "$plugin" | sed -n 's/^ *Machine: *//p')
[[ $machine == "$expected_machine" ]]
python3 - "$manifest" "$data_dir" <<'PY'
import hashlib
import json
import pathlib
import sys

manifest_path = pathlib.Path(sys.argv[1])
data_dir = pathlib.Path(sys.argv[2])
manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
if manifest.get("dataset_version") != "ocpn-climatology-2026.2":
    raise SystemExit("unexpected Climatology dataset version")
for output in manifest.get("outputs", []):
    path = data_dir / output["file"]
    if not path.is_file():
        raise SystemExit(f"missing Climatology dataset output: {path.name}")
    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    if digest != output["sha256"]:
        raise SystemExit(f"Climatology checksum mismatch: {path.name}")
PY

tar -C "$stage_dir" -czf "$output_archive" usr/local
printf 'Created qualified Climatology package: %s\n' "$output_archive"
