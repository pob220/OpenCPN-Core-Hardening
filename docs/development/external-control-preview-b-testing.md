# External-control Preview B.1 hardening developer test guide

Preview B.1 is a Linux x86_64 developer candidate. It must be installed in an
isolated directory with an isolated OpenCPN configuration. Do not overwrite a
working or onboard installation, and do not use Preview B.1 as the sole source
of navigation information.

## Published sources and packages

- OpenCPN core: `pob220/OpenCPN-Core-Hardening`, branch
  `external-control/preview-b.1-hardening`;
- xWeatherRouting: `pob220/xweather_routing_pi`, branch
  `external-control/preview-b`;
- xGRIB: `pob220/xgrib_pi`, branch `main`;
- release: `external-control-preview-b1-hardening-20260820` in the core
  repository.

The release includes a complete OpenCPN self-extracting package and
resource-complete xWeatherRouting and xGRIB archives. The raw executable and
shared libraries are diagnostic build artifacts, not complete installations.
Verify every downloaded file with `SHA256SUMS` before using it.

`OpenCPN-5.15.0-Preview-B1-Hardening-complete-linux-x86_64.tar.gz` is the
single-download form of the same installer, both plug-in archives, Python/MCP
packages, API contract, checksums and this guide. Its contents are not a
different build.

The package extraction layout is deliberately visible and does not modify the
system package database. The xWeatherRouting archive includes its locales,
data, example boats and polars. The xGRIB archive includes its plug-in data and
private helper runtime.

## Make an isolated installation

Download these four release assets into an empty directory:

```text
opencpn-5.15.0-preview-b1-hardening-linux-x86_64-installer.sh
xweather-routing-preview-b1-hardened-arch-x86_64.tar.gz
xgrib-preview-b1-0.2.4.0-arch-x86_64.tar.gz
SHA256SUMS
```

Then use a new test root. The example below does not use `sudo` and does not
write to `/usr/local` on the host:

```sh
sha256sum --check SHA256SUMS --ignore-missing
export OCPN_PREVIEW_ROOT="$PWD/Test-OpenCPN-Preview-B1-Hardening"
mkdir -p "$OCPN_PREVIEW_ROOT/usr/local" "$OCPN_PREVIEW_ROOT/config"

chmod u+x opencpn-5.15.0-preview-b1-hardening-linux-x86_64-installer.sh
./opencpn-5.15.0-preview-b1-hardening-linux-x86_64-installer.sh \
  --skip-license --exclude-subdir --prefix="$OCPN_PREVIEW_ROOT"

tar -xzf xweather-routing-preview-b1-hardened-arch-x86_64.tar.gz \
  -C "$OCPN_PREVIEW_ROOT" --strip-components=1
tar -xzf xgrib-preview-b1-0.2.4.0-arch-x86_64.tar.gz \
  -C "$OCPN_PREVIEW_ROOT/usr/local" --strip-components=1
```

The expected executable is
`$OCPN_PREVIEW_ROOT/usr/local/bin/opencpn`. The core installer has been checked
by extracting all 790 core payload files into an unprivileged isolated
directory. The two plug-in archives were then extracted over that test root
and their installed shared libraries verified.

Start it once, configure a test chart directory, disable the bundled **GRIB**
plug-in, enable **xGRIB** and **xWeatherRouting**, then close OpenCPN. Do not
enable the bundled GRIB and xGRIB together. Likewise, do not enable the
original Weather Routing plug-in and xWeatherRouting together.

```sh
"$OCPN_PREVIEW_ROOT/usr/local/bin/opencpn" \
  --portable --configdir "$OCPN_PREVIEW_ROOT/config" --no_opengl
```

## Enable the local API

Generate a random bearer token without putting it in shell history:

```sh
read -rsp 'Preview token: ' OPENCPN_TOKEN; echo
printf %s "$OPENCPN_TOKEN" | sha256sum
export OPENCPN_TOKEN
export OPENCPN_URL=https://127.0.0.1:8443
```

Add the following section to the isolated `opencpn.conf`, using the lower-case
digest printed above. Keep `AllowLan=0`:

```ini
[Settings/ExternalControl]
Enabled=1
AllowLan=0
TokenSha256=<lower-case SHA-256 digest>
TokenScopes=navigation:read;routes:read;charts:query;planning:run;routes:write
MaximumBodyBytes=1048576
```

Route activation is deliberately absent from this ordinary test token. If it
must be tested, use a second isolated profile and explicitly add
`routes:activate`; the CLI also requires `--confirm`.

Restart Preview B.1. OpenCPN creates its self-signed development certificate in
the isolated configuration directory. Install the SDK wheel in a virtual
environment and perform the first checks:

```sh
python3 -m venv preview-client
preview-client/bin/pip install opencpn_control-0.1.0-py3-none-any.whl
preview-client/bin/opencpnctl --insecure status
preview-client/bin/opencpnctl --insecure routes list
```

`--insecure` is only for this loopback test server and its generated
self-signed certificate. It is not a production TLS configuration.

The status response must advertise both
`route-planning.chart-direct.v1` and `route-planning.chart-weather.v1`. If the
weather capability is absent, confirm that xWeatherRouting is enabled and
inspect the OpenCPN log for the provider-registration message.

## Exercise chart/weather planning

Select a real polar in xWeatherRouting and load the weather/current data to be
used. Preview B supports the currently active datasets; it does not yet expose
a dataset or polar catalogue. Use the installed polar file name as
`polarIdentity`, not a filesystem path.

Create `scenario.json`:

```json
{
  "providerCapability": "route-planning.chart-weather.v1",
  "start": {"latitudeDegrees": 53.31, "longitudeDegrees": -4.67},
  "destination": {"latitudeDegrees": 55.17, "longitudeDegrees": -6.92},
  "departureTimeUtc": "2026-08-20T12:00:00Z",
  "minimumDepthMeters": 5.0,
  "landMarginNauticalMiles": 0.1,
  "polarIdentity": "Boat.xml",
  "weatherDatasetIdentity": "active",
  "currentDatasetIdentity": "active",
  "horizonHours": 240,
  "allowClimatologyFallback": false,
  "effortLimit": 1000000,
  "departureWindowBeforeMinutes": 180,
  "departureWindowAfterMinutes": 180,
  "departureStepMinutes": 60,
  "concurrentRoutes": 7,
  "routingEffortPercent": 200
}
```

Replace the date, coordinates and polar with a scenario covered by the active
forecast. A horizon longer than 128 hours is valid; Preview B allows up to one
year. Climatology is used only when `allowClimatologyFallback` is explicitly
true and a compatible Climatology plug-in is available.

```sh
JOB_ID=$(preview-client/bin/opencpnctl --insecure planning submit scenario.json \
  | python3 -c 'import json,sys; print(json.load(sys.stdin)["id"])')
preview-client/bin/opencpnctl --insecure planning watch "$JOB_ID"
```

For a deliberately long job, verify cancellation separately:

```sh
preview-client/bin/opencpnctl --insecure planning cancel "$JOB_ID"
preview-client/bin/opencpnctl --insecure planning watch "$JOB_ID"
```

A successful result must contain the complete start-to-destination geometry,
active-data provenance, and `finalSafety` with `authority` equal to
`authoritative` and `decision` equal to `pass`. It remains a draft and is not
activated. A land crossing, unknown depth, unavailable chart evidence,
missing polar, or invalid dataset identity must fail explicitly; none may be
reported as a passing draft.

Also submit a small known-clear request immediately after a cancelled job. It
must not fail with `provider_busy`. This checks resident-provider cleanup.

## Build the same candidate from source

Core:

```sh
git clone --branch external-control/preview-b.1-hardening \
  https://github.com/pob220/OpenCPN-Core-Hardening.git opencpn-preview-b
cmake -S opencpn-preview-b -B opencpn-preview-b/build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOCPN_BUILD_TEST=ON
cmake --build opencpn-preview-b/build --parallel
ctest --test-dir opencpn-preview-b/build/test -j1 --output-on-failure \
  -E '^(IpcClient\.|IpcServer\.Commands$|tests$)'
cpack --config opencpn-preview-b/build/CPackConfig.cmake \
  -D CPACK_SET_DESTDIR=ON
```

The exclusion matches the three desktop-session-dependent `IpcClient.*`
tests, `IpcServer.Commands`, and the historical aggregate wrapper. The 119
independently runnable registrations must pass sequentially; some older tests
share `opencpn.conf` and are not parallel-safe.

xWeatherRouting stock-host compatibility and hardened package:

```sh
git clone --branch external-control/preview-b \
  https://github.com/pob220/xweather_routing_pi.git xweather-routing-preview-b

cmake -S xweather-routing-preview-b -B xweather-routing-preview-b/build-test \
  -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DXWEATHER_ROUTING_STANDALONE_API=ON -DOCPN_BUILD_TEST=ON
cmake --build xweather-routing-preview-b/build-test --parallel
ctest --test-dir xweather-routing-preview-b/build-test --output-on-failure

cmake -S xweather-routing-preview-b \
  -B xweather-routing-preview-b/build-hardened -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOCPN_BUILD_TEST=OFF \
  -DOCPN_IN_TREE_SOURCE_DIR="$PWD/opencpn-preview-b"
cmake --build xweather-routing-preview-b/build-hardened --parallel
cmake --build xweather-routing-preview-b/build-hardened --target package
```

xGRIB:

```sh
git clone --recurse-submodules https://github.com/pob220/xgrib_pi.git
cmake -S xgrib_pi -B xgrib_pi/build -DCMAKE_BUILD_TYPE=Release \
  -DBUNDLE_GENERATOR_RUNTIME=ON
cmake --build xgrib_pi/build --parallel
ctest --test-dir xgrib_pi/build --output-on-failure
cmake --build xgrib_pi/build --target package
```

Platform development dependencies are not vendored by the OpenCPN core. Use
the normal OpenCPN build prerequisites for the target distribution. xGRIB has
additional generator dependencies documented in its repository.

## What feedback is useful

Report the exact core and plug-in commits, platform and wxWidgets version,
chart type and coverage, polar identity, weather/current provenance, request
JSON with secrets removed, job state/error, and the relevant OpenCPN log. Do
not publish bearer tokens, private chart paths or licensed chart data.

Particularly useful Preview B evidence is provider cold-start registration,
cancel-then-reuse, plug-in disable/unload during or after planning, a completed
coastal route with independent 5 m validation, and fail-closed behavior where
chart or depth evidence is unavailable.

The complete qualification record is
[external-control-qualification.md](external-control-qualification.md), and
the normative wire contract is [api/openapi-v2.yaml](../../api/openapi-v2.yaml).
