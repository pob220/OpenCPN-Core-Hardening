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
    for _attempt in $(seq 1 50); do
      if ! kill -0 -- "-$smoke_pid" 2>/dev/null; then
        break
      fi
      sleep 0.1
    done
    kill -KILL -- "-$smoke_pid" 2>/dev/null || true
    wait "$smoke_pid" 2>/dev/null || true
  fi
  for _attempt in $(seq 1 10); do
    if rm -rf "$work_dir"; then
      return
    fi
    sleep 0.1
  done
  rm -rf "$work_dir"
}
trap cleanup EXIT

unexpected_error() {
  local status=$?
  echo "Qualification command failed at line $1 (status $status): $2" >&2
  exit "$status"
}
trap 'unexpected_error "$LINENO" "$BASH_COMMAND"' ERR

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
require_executable "$install_dir/bin/launch-opencpn-mcp-external-control-demo"
require_executable "$install_dir/client/bin/opencpnctl"
require_executable "$install_dir/client/bin/opencpn-mcp"
require_executable "$install_dir/client/bin/opencpn-scheduler"
if grep -R -F "${install_dir}.installing." "$install_dir/client/bin" \
    >/dev/null 2>&1; then
  fail 'installed Python entry points retain the atomic staging path'
fi
require_mode "$install_dir/config/opencpn.conf" 600
require_mode "$install_dir/secrets/api-token" 600
grep -q '^AllowLan=0$' "$install_dir/config/opencpn.conf" ||
  fail 'external API is not restricted to loopback'
grep -q '^TokenScopes=' "$install_dir/config/opencpn.conf" ||
  fail 'external API token scopes are missing'
grep -q '^AllowArbitrarySystemPlugins=1$' \
  "$install_dir/config/opencpn.conf" ||
  fail 'pinned bundled plug-ins are not explicitly allowed by the demo profile'
python3 - "$install_dir/config/opencpn.conf" <<'PY'
import configparser
import sys

config = configparser.RawConfigParser()
config.optionxform = str
config.read(sys.argv[1], encoding="utf-8")
expected = {
    "PlugIns/libgrib_pi.so": "0",
    "PlugIns/libxgrib_pi.so": "1",
    "PlugIns/libxweather_routing_pi.so": "1",
    "PlugIns/libo-charts_pi.so": "1",
    "PlugIns/libclimatology_pi.so": "1",
    "PlugIns/libpolar_pi.so": "1",
    "PlugIns/libcelestial_navigation_pi.so": "1",
}
for section, value in expected.items():
    actual = config.get(section, "bEnabled", fallback=None)
    if actual != value:
        raise SystemExit(
            f"unexpected initial plug-in state for {section}: {actual!r}"
        )
PY
if grep -Eq 'routes:activate|autopilot|messages:send' \
  "$install_dir/config/opencpn.conf"; then
  echo 'Generated token contains a prohibited navigation-control scope.' >&2
  exit 1
fi

"$source_root/ci/external-control-demo/verify-linux-architecture.sh" \
  "$install_dir/usr/local" "$expected_machine"

required_plugins=(
  libchartdldr_pi.so
  libdashboard_pi.so
  libgrib_pi.so
  libwmm_pi.so
  libxgrib_pi.so
  libxweather_routing_pi.so
  libo-charts_pi.so
  libclimatology_pi.so
  libpolar_pi.so
  libcelestial_navigation_pi.so
)
for plugin in "${required_plugins[@]}"; do
  [[ -f "$install_dir/usr/local/lib/opencpn/$plugin" ]] ||
    fail "installed plug-in is missing: $plugin"
done
for data_dir in chartdldr_pi dashboard_pi grib_pi wmm_pi xgrib_pi \
  xweather_routing_pi o-charts_pi climatology_pi polar_pi \
  celestial_navigation_pi; do
  [[ -d "$install_dir/usr/local/share/opencpn/plugins/$data_dir" ]] ||
    fail "installed plug-in data is missing: $data_dir"
done
[[ -x "$install_dir/usr/local/bin/oexserverd" ]] ||
  fail 'bundled o-charts licensed helper is missing or not executable'
readelf -Ws "$install_dir/usr/local/lib/opencpn/libo-charts_pi.so" |
  grep -F 'OCPN_PluginChartSafetyGridV1' >/dev/null ||
  fail 'bundled o-charts does not export the chart-safety batch provider'
readelf -Ws "$install_dir/usr/local/lib/opencpn/libo-charts_pi.so" |
  grep -F 'OCPN_PluginChartSafetyIdentityV1' >/dev/null ||
  fail 'bundled o-charts does not export a stable chart-safety identity'
[[ -f "$install_dir/docs/PLUGIN-INVENTORY.md" ]] ||
  fail 'installed plug-in inventory is missing'
[[ -f "$install_dir/docs/FULL-SYSTEM-CHECKLIST.md" ]] ||
  fail 'installed full-system checklist is missing'
[[ -f "$install_dir/docs/openapi-v2.yaml" ]] ||
  fail 'installed OpenAPI contract is missing'
[[ -f "$install_dir/docs/scheduler-schedule-v1.schema.json" ]] ||
  fail 'installed Scheduler JSON schema is missing'
[[ -f "$install_dir/docs/scheduler-contract.md" ]] ||
  fail 'installed Scheduler contract is missing'
[[ -f "$install_dir/docs/CELESTIAL-ECLIPSE-DATA.md" ]] ||
  fail 'installed optional Celestial eclipse-data guidance is missing'
[[ -f "$install_dir/docs/CHART-AWARE-ROUTING-PREVIEW.md" ]] ||
  fail 'installed chart-aware routing test guidance is missing'
grep -q 'requests additional fail-closed safety' \
  "$install_dir/docs/CHART-AWARE-ROUTING-PREVIEW.md" ||
  fail 'chart-aware routing guidance does not describe on-demand expansion'
grep -q 'eclipse-data-2026.1' \
  "$install_dir/docs/CELESTIAL-ECLIPSE-DATA.md" ||
  fail 'Celestial eclipse-data guidance does not link the pinned data release'
"$install_dir/client/bin/python" -c \
  'import opencpn_control, opencpn_mcp, opencpn_scheduler' ||
  fail 'installed Python client packages cannot be imported'
"$install_dir/client/bin/opencpn-scheduler" --help \
  >"$work_dir/scheduler-help.txt"
grep -q 'opencpn-scheduler' "$work_dir/scheduler-help.txt" ||
  fail 'installed Scheduler entry point is not usable'
printf '%s\n' \
  '{"jsonrpc":"2.0","id":1,"method":"server/discover","params":{}}' |
  OPENCPN_TOKEN=qualification-only OPENCPN_INSECURE=1 \
  "$install_dir/client/bin/opencpn-mcp" >"$work_dir/mcp-discover.json"
python3 - "$work_dir/mcp-discover.json" <<'PY'
import json
import sys

response = json.load(open(sys.argv[1], encoding="utf-8"))
assert response["id"] == 1
assert "2026-07-28" in response["result"]["protocolVersions"]
PY

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
  pixbuf_cache=$(find /usr/lib -type f \
    -path '*/gdk-pixbuf-2.0/2.10.0/loaders.cache' -print -quit)
  [[ -n $pixbuf_cache ]] || fail 'GDK pixbuf loader cache is unavailable'
  svg_icon="$install_dir/usr/local/share/opencpn/plugins/celestial_navigation_pi/data/celestial_navigation.svg"
  if command -v gdk-pixbuf-thumbnailer >/dev/null 2>&1; then
    GDK_PIXBUF_MODULE_FILE="$pixbuf_cache" gdk-pixbuf-thumbnailer \
      --size 32 "$svg_icon" "$work_dir/pixbuf-svg-canary.png"
  elif command -v rsvg-convert >/dev/null 2>&1; then
    rsvg-convert --width 32 --height 32 \
      --output "$work_dir/pixbuf-svg-canary.png" "$svg_icon"
  else
    fail 'neither GDK pixbuf nor librsvg can render the Celestial SVG icon'
  fi
  file -b "$work_dir/pixbuf-svg-canary.png" | grep -q '^PNG image data' ||
    fail 'GDK pixbuf could not render the bundled Celestial SVG icon'
  smoke_display=(xvfb-run -a)
  if ! command -v xvfb-run >/dev/null 2>&1; then
    [[ -n ${DISPLAY:-} ]] || fail 'xvfb-run is unavailable and no display is active'
    smoke_display=()
  fi
  setsid env XDG_RUNTIME_DIR="$work_dir/runtime" \
    GDK_PIXBUF_MODULE_FILE="$pixbuf_cache" \
    OCPN_EXTERNAL_CONTROL_DEMO_QUALIFICATION=1 \
    dbus-run-session -- "${smoke_display[@]}" \
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
    if ! kill -0 "$smoke_pid" 2>/dev/null; then
      echo 'The isolated OpenCPN process exited before API readiness.' >&2
      break
    fi
    sleep 1
  done
  if (( ready == 0 )); then
    cat "$work_dir/opencpn-smoke.log" >&2
    while IFS= read -r log_file; do
      echo "OpenCPN log: $log_file" >&2
      tail -200 "$log_file" >&2
    done < <(find "$install_dir" -type f -name opencpn.log -print)
    echo 'The isolated External Control Demo API did not become ready.' >&2
    exit 1
  fi
  listeners=$(ss -H -ltn 'sport = :8443')
  [[ -n $listeners ]] || fail 'external-control listener is absent'
  if grep -Eq '(^|[[:space:]])(0\.0\.0\.0|\*|\[::\]):8443([[:space:]]|$)' \
      <<<"$listeners"; then
    echo "$listeners" >&2
    fail 'safe-default demo listener is exposed beyond loopback'
  fi
  grep -Eq '(^|[[:space:]])127\.0\.0\.1:8443([[:space:]]|$)' \
    <<<"$listeners" || fail 'external-control listener is not bound to IPv4 loopback'
  python3 -m json.tool "$work_dir/version.json" >/dev/null ||
    fail 'version endpoint did not return valid JSON'
  OPENCPN_TOKEN="$token" "$install_dir/client/bin/opencpnctl" \
    --url https://127.0.0.1:8443 --insecure status \
    >"$work_dir/opencpnctl-status.json"
  python3 -m json.tool "$work_dir/opencpnctl-status.json" >/dev/null ||
    fail 'opencpnctl status did not return valid JSON'
  if ! curl --silent --show-error --insecure --fail --max-time 5 \
      --header "Authorization: Bearer $token" \
      https://127.0.0.1:8443/api/v2/capabilities \
      >"$work_dir/capabilities.json"; then
    cat "$work_dir/opencpn-smoke.log" >&2
    while IFS= read -r log_file; do
      echo "OpenCPN log: $log_file" >&2
      tail -200 "$log_file" >&2
    done < <(find "$install_dir" -type f -name opencpn.log -print)
    fail 'capabilities endpoint was unavailable after version readiness'
  fi
  python3 - "$work_dir/capabilities.json" <<'PY'
import json
import sys

capabilities = set(json.load(open(sys.argv[1], encoding="utf-8"))["capabilities"])
required = {
    "environmental-data.xgrib.v1",
    "route-planning.chart-weather.v1",
}
missing = sorted(required - capabilities)
if missing:
    raise SystemExit("missing enabled provider capabilities: " + ", ".join(missing))
PY
  printf '%s\n' \
    '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"get_opencpn_status","arguments":{}}}' |
    "$install_dir/bin/launch-opencpn-mcp-external-control-demo" \
    >"$work_dir/mcp-status.json"
  python3 - "$work_dir/mcp-status.json" <<'PY'
import json
import sys

response = json.load(open(sys.argv[1], encoding="utf-8"))
if response.get("result", {}).get("isError"):
    raise SystemExit(response)
content = response["result"]["structuredContent"]["result"]
if "version" not in content or "capabilities" not in content:
    raise SystemExit("MCP status response is incomplete")
PY
fi

echo 'External Control Demo Linux bundle qualification passed.'
