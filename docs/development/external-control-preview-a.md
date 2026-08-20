# External-control Preview A

Preview A is an opt-in developer build for evaluating OpenCPN's external
control boundary. It is not a navigation release and must not replace a
known-good onboard installation.

## Included vertical slice

- disabled-by-default, authenticated `/api/v2` HTTPS API;
- navigation, route and active-route snapshots;
- authoritative, fail-closed chart point/segment/route safety queries;
- guarded and transactional external-control draft operations;
- bounded semantic WebSocket events;
- cancellable asynchronous planning jobs;
- the conservative built-in `route-planning.chart-direct.v1` provider;
- dependency-free Python SDK and `opencpnctl`;
- least-privilege stdio MCP adapter.

The chart-direct provider validates a direct segment. It is deliberately not a
weather optimizer. xWeatherRouting registration is the Preview B objective.

## Isolated setup

Use a separate HOME/XDG profile, chart database, port and plugin directory.
Do not point a preview at the profile used aboard. The API is disabled until
all of the following are configured under `/Settings/ExternalControl`:

```ini
Enabled=1
AllowLan=0
TokenSha256=<lower-case SHA-256 of a random bearer token>
TokenScopes=navigation:read;routes:read;charts:query;planning:run;routes:write
MaximumBodyBytes=1048576
```

Generate a token without putting it in shell history:

```sh
read -rsp 'Preview token: ' OPENCPN_TOKEN; echo
printf %s "$OPENCPN_TOKEN" | sha256sum
export OPENCPN_TOKEN
export OPENCPN_URL=https://127.0.0.1:8443
```

Keep `AllowLan=0`. Route activation needs a separately scoped token containing
`routes:activate`; do not grant that scope to ordinary SDK or MCP clients.

## First checks

Build and install into the isolated prefix, then install the SDK from
`external/opencpn-control-python` and run:

```sh
opencpnctl --insecure status
opencpnctl --insecure routes list
opencpnctl --insecure events watch navigation active-route
```

The self-signed development certificate is the only reason to use
`--insecure`. A real deployment must use a verified certificate.

The wire contract is [api/openapi-v2.yaml](../../api/openapi-v2.yaml). The
architecture, threat model, qualification evidence and known limitations are
recorded in the adjacent development documents.
