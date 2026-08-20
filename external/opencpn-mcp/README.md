# OpenCPN MCP adapter

A separate stdio MCP server which depends only on `opencpn-control`. It
supports the current stateless MCP revision and the preceding initialization
flow for compatibility with installed hosts.

```sh
export OPENCPN_URL=https://127.0.0.1:8443
export OPENCPN_TOKEN=...
opencpn-mcp
```

The initial tool set covers status, navigation, route inspection and
validation, active-route inspection, planning submit/status/cancel/compare,
and optional draft creation. Planning jobs live in OpenCPN, so an MCP adapter
restart does not terminate them.

Default credentials should contain only `navigation:read`, `routes:read`,
`charts:query`, and—when planning is installed—`planning:run`. Draft creation
requires a separately enabled `routes:write` scope. The adapter deliberately
does not expose route activation, raw plugin messages, autopilot output,
configuration, plugin management, or filesystem access.
