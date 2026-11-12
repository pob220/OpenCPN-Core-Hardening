# xGRIB 0.4.4 developer-bundle integration

The bundle pins preview xGRIB `2bee0bc8ce68f6da0f70abbfcd6fa03f1f4f3c39` (version 0.4.4), based on
ordinary xGRIB `ea377c6afefe40ea6ef023bf19903fbecb427d28`. Generator 0.4.0 remains pinned at
`4208c87dbf223bedb78064043313c5fc09c629f1`.

The only adapter changes are the existing optional environmental-data provider,
ported from the previously qualified 0.4.0 preview. It retains external
acquisition, cancellation and provenance for the preview API, Python SDK and
MCP tools. The ordinary working xGRIB installation is unchanged.

This refresh retains gust-capable Routing forecasts and the 0.4.4 early
Copernicus NWS time-range validation. Both architectures must pass the complete
plugin tests, clean bundle installation, provider registration and API smoke
checks. Hosted GFS fallback still requires service 0.2.2 and its five-field
weather cache. No service deployment is part of this refresh.
