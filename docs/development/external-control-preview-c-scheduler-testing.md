# External-control Preview C Scheduler developer test guide

Preview C is a Linux x86_64 developer demonstration.  Install it in a new
directory with a new OpenCPN configuration.  Do not overwrite a working or
onboard installation, and do not treat Preview C as a navigation release.

## Published components

- core: `pob220/OpenCPN-Core-Hardening`, branch
  `external-control/preview-c-scheduler`;
- xGRIB: `pob220/xgrib_pi`, branch
  `external-control/scheduler-preview`;
- xWeatherRouting: `pob220/xweather_routing_pi`, branch
  `external-control/scheduler-preview`;
- Scheduler: `pob220/opencpn-scheduler`, branch `main`.

The complete archive contains the OpenCPN installer, both resource-complete
plug-in archives, the Python SDK and Scheduler packages, the OpenAPI and
schedule schemas, this guide and `SHA256SUMS`.

## Isolated installation

```sh
sha256sum --check SHA256SUMS --ignore-missing
export OCPN_PREVIEW_ROOT="$PWD/Test-OpenCPN-Preview-C"
mkdir -p "$OCPN_PREVIEW_ROOT/usr/local" "$OCPN_PREVIEW_ROOT/config"

chmod u+x opencpn-5.15.0-preview-c-scheduler-linux-x86_64-installer.sh
./opencpn-5.15.0-preview-c-scheduler-linux-x86_64-installer.sh \
  --skip-license --exclude-subdir --prefix="$OCPN_PREVIEW_ROOT"
tar -xzf xweather-routing-preview-c-scheduler-arch-x86_64.tar.gz \
  -C "$OCPN_PREVIEW_ROOT" --strip-components=1
tar -xzf xgrib-preview-c-scheduler-0.2.4.0-arch-x86_64.tar.gz \
  -C "$OCPN_PREVIEW_ROOT/usr/local" --strip-components=1

OCPN_PREVIEW_ROOT="$OCPN_PREVIEW_ROOT" ./run-preview-c.sh
```

On first start, select test charts, enable **xGRIB** and
**xWeatherRouting**, and disable the bundled **GRIB** and original Weather
Routing plug-ins.  The launcher sets `OPENCPN_PREFIX` and an isolated
`--configdir`; it does not modify the host installation.

## API credential

Generate a random token and put only its SHA-256 digest in the isolated
`opencpn.conf`:

```ini
[Settings/ExternalControl]
Enabled=1
AllowLan=0
TokenSha256=<lower-case SHA-256 digest>
TokenScopes=navigation:read;routes:read;routes:write;charts:query;planning:run;environment:read;environment:acquire;environment:activate
MaximumBodyBytes=1048576
```

Deliberately omit `routes:activate`.  Preview C creates and replaces only its
own draft route; it cannot activate a route or produce autopilot output.
The generated self-signed certificate and `--insecure` client flag are for
isolated loopback testing only.

## Install and run the Scheduler

```sh
python3 -m venv preview-c-client
preview-c-client/bin/pip install \
  opencpn_control-0.2.0-py3-none-any.whl \
  opencpn_scheduler-0.1.0-py3-none-any.whl
export OPENCPN_URL=https://127.0.0.1:8443
read -rsp 'Preview token: ' OPENCPN_TOKEN; echo
export OPENCPN_TOKEN
preview-c-client/bin/opencpn-scheduler --insecure providers
preview-c-client/bin/opencpn-scheduler --insecure gui
```

Provider discovery must show `environmental-data.xgrib.v1` and
`route-planning.chart-weather.v1`, including typed xGRIB fields and installed
boat resources.  The GUI's two example buttons create disabled schedules:
review their area, forecast sources, installed polar, coordinates and minimum
depth before enabling them.

The route workflow must acquire and validate an immutable xGRIB dataset,
activate that exact dataset for display, pin the same identity into
xWeatherRouting, require OpenCPN's independent `authoritative/pass` chart
safety result, and transactionally publish a draft.  Restart both programs
and confirm schedules, history and the owned draft persist.

## Expected failure behaviour

Malformed provider data, stale ownship, incomplete acquisition, an unknown
dataset, missing polar, land crossing, unknown depth or failed chart evidence
must fail explicitly.  The previous displayed dataset and last known-good
draft remain intact.  Cancelling or unloading a provider must reach a bounded
terminal state without callbacks outliving the provider.

Useful reports include exact commits, platform/wx version, chart coverage,
polar identity, forecast provenance, request with secrets removed, terminal
job error and relevant OpenCPN log.  Never publish tokens, private chart paths
or licensed chart data.

