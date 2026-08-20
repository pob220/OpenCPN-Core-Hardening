#!/bin/sh

set -eu

: "${OCPN_PREVIEW_ROOT:?Set OCPN_PREVIEW_ROOT to the isolated package root}"

preview_prefix="$OCPN_PREVIEW_ROOT/usr/local"
preview_config="${OCPN_PREVIEW_CONFIG_DIR:-$OCPN_PREVIEW_ROOT/config}"
preview_executable="$preview_prefix/bin/opencpn"

if [ ! -x "$preview_executable" ]; then
  echo "Preview B.1 executable not found: $preview_executable" >&2
  exit 1
fi

mkdir -p "$preview_config"
export OPENCPN_PREFIX="$preview_prefix"

exec "$preview_executable" --configdir "$preview_config" --no_opengl "$@"
