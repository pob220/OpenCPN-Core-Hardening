#!/usr/bin/env bash
set -euo pipefail

bundle_dir=$(realpath "${1:?usage: qualify-linux-bundle.sh BUNDLE-DIRECTORY EXPECTED-ELF-MACHINE}")
expected_machine=${2:?usage: qualify-linux-bundle.sh BUNDLE-DIRECTORY EXPECTED-ELF-MACHINE}
source_root=$(git -C "$(dirname -- "$0")/../.." rev-parse --show-toplevel)
work_dir=$(mktemp -d "${TMPDIR:-/tmp}/external-control-demo-qualification.XXXXXX")
install_dir="$work_dir/install"
smoke_pid=

cleanup() {
  if [[ -n "$smoke_pid" ]]; then
    kill -- "-$smoke_pid" 2>/dev/null || true
    wait "$smoke_pid" 2>/dev/null || true
  fi
  rm -rf "$work_dir"
}
trap cleanup EXIT

fail() {
  echo "Qualification failed: $*" >&2
  exit 1
}

require_executable() {
  [[ -x $1 ]] || fail "expected executable is missing: $1"
}

require_mode() {
  local path=$1
  local expected=$2
  local actual
  actual=$(stat -c %a "$path") || fail "cannot read mode for $path"
  [[ $actual == "$expected" ]] ||
    fail "expected mode $expected for $path; found $actual"
}

(cd "$bundle_dir" && sha256sum --check SHA256SUMS)
"$bundle_dir/install-external-control-demo.sh" "$install_dir"

require_executable "$install_dir/usr/local/bin/opencpn"
require_executable "$install_dir/bin/launch-opencpn-external-control-demo"
require_executable "$install_dir/bin/launch-opencpn-scheduler-external-control-demo"
require_mode "$install_dir/config/opencpn.conf" 600
require_mode "$install_dir/secrets/api-token" 600
grep -q '^AllowLan=0$' "$install_dir/config/opencpn.conf" ||
  fail 'external API is not restricted to loopback'
grep -q '^TokenScopes=' "$install_dir/config/opencpn.conf" ||
  fail 'external API token scopes are missing'
if grep -Eq 'routes:activate|autopilot|messages:send' \
  "$install_dir/config/opencpn.conf"; then
  echo 'Generated token contains a prohibited navigation-control scope.' >&2
  exit 1
fi

"$source_root/ci/external-control-demo/verify-linux-architecture.sh" \
  "$install_dir/usr/local" "$expected_machine"
"$install_dir/client/bin/python" -c \
  'import opencpn_control, opencpn_scheduler'

if "$bundle_dir/install-external-control-demo.sh" "$install_dir" \
  >"$work_dir/second-install.log" 2>&1; then
  echo 'A second install unexpectedly overwrote the qualified target.' >&2
  exit 1
fi
grep -q 'Refusing to overwrite' "$work_dir/second-install.log" ||
  fail 'second-install refusal did not report its reason'

if [[ ${RUN_GUI_SMOKE:-0} == 1 ]]; then
  token=$(<"$install_dir/secrets/api-token")
  mkdir -p "$work_dir/runtime"
  chmod 700 "$work_dir/runtime"
  setsid env XDG_RUNTIME_DIR="$work_dir/runtime" dbus-run-session -- xvfb-run -a \
    "$install_dir/bin/launch-opencpn-external-control-demo" \
    >"$work_dir/opencpn-smoke.log" 2>&1 &
  smoke_pid=$!
  ready=0
  for _attempt in $(seq 1 90); do
    if curl --silent --insecure --fail --max-time 2 \
      --header "Authorization: Bearer $token" \
      https://127.0.0.1:8443/api/v2/version \
      >"$work_dir/version.json"; then
      ready=1
      break
    fi
    sleep 1
  done
  if (( ready == 0 )); then
    cat "$work_dir/opencpn-smoke.log" >&2
    echo 'The isolated External Control Demo API did not become ready.' >&2
    exit 1
  fi
  python3 -m json.tool "$work_dir/version.json" >/dev/null
fi

echo 'External Control Demo Linux bundle qualification passed.'
