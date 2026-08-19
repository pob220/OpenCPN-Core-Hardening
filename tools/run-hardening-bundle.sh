#!/bin/sh

# Launcher copied to the root of the Linux hardening tester bundle.  Keep the
# package isolated from an installed OpenCPN and from the user's live profile.

set -eu

bundle_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
expected_root=/tmp/opencpn-hardening-install

if [ "$bundle_root" != "$expected_root" ]; then
  echo "This host-built tester must be extracted as $expected_root." >&2
  echo "See HARDENING.md in the bundle before running it." >&2
  exit 2
fi

config_dir=${OCPN_HARDENING_CONFIG_DIR:-$bundle_root/profile}
mkdir -p "$config_dir"

export OPENCPN_PLUGIN_DIRS="$bundle_root/extra-plugins:$bundle_root/lib/opencpn"
export XDG_DATA_DIRS="$bundle_root/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"

exec "$bundle_root/bin/opencpn" -c "$config_dir" "$@"
