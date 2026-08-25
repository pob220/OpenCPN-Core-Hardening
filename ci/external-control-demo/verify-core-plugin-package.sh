#!/usr/bin/env bash
set -euo pipefail

if (( $# != 3 )); then
  echo "usage: verify-core-plugin-package.sh STAGE-DIRECTORY PACKAGE-ARCHIVE EXPECTED-ELF-MACHINE" >&2
  exit 2
fi

stage_dir=$(realpath "$1")
package_archive=$(realpath "$2")
expected_machine=$3

fail() {
  echo "Bundled plug-in package audit failed: $*" >&2
  exit 1
}

[[ -d $stage_dir ]] || fail "stage directory is missing: $stage_dir"
[[ -f $package_archive ]] || fail "package archive is missing: $package_archive"

plugins=(
  libchartdldr_pi.so
  libdashboard_pi.so
  libgrib_pi.so
  libwmm_pi.so
)

archive_listing=$(tar -tzf "$package_archive") ||
  fail "cannot inspect $package_archive"

for plugin in "${plugins[@]}"; do
  staged=$(find "$stage_dir" -type f -path "*/lib/opencpn/$plugin" -print -quit)
  [[ -n $staged ]] || fail "$plugin is absent from the staged install"
  machine=$(readelf -h "$staged" | sed -n 's/^ *Machine: *//p')
  [[ $machine == "$expected_machine" ]] ||
    fail "$plugin is not an $expected_machine binary: $machine"
  grep -Eq "(^|/)lib/opencpn/${plugin}$" <<<"$archive_listing" ||
    fail "$plugin is absent from the distributable archive"
done

printf 'Bundled OpenCPN plug-in package audit passed (%s):\n' "$expected_machine"
printf '  %s\n' "${plugins[@]}"
