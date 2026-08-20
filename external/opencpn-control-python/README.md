# OpenCPN control client

A dependency-free Python client and `opencpnctl` command-line interface for
OpenCPN's versioned external-control API.

The token is read from `OPENCPN_TOKEN`; passing it on the command line is
supported for automation but discouraged because shell history and process
listings can expose it.

```sh
export OPENCPN_URL=https://127.0.0.1:8443
export OPENCPN_TOKEN=...
opencpnctl status
opencpnctl routes list
opencpnctl routes validate ROUTE_GUID --minimum-depth-m 5
```

Use `--insecure` only with the isolated development server's self-signed
certificate. Route activation and deactivation require both a separately
scoped token and an explicit `--confirm` flag.
