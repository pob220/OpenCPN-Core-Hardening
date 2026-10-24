# xGRIB 0.4.0 developer-bundle integration

The bundle pins preview xGRIB `c56d6ff32696da83c0af75d3e4bacd95943e84e0`
(version 0.4.0), based on ordinary xGRIB `81add3e02bba645beeaed6a9fe5a098d087cc6c8`.
Generator 0.4.0 is pinned at `4208c87dbf223bedb78064043313c5fc09c629f1`.

The preview retains the optional environmental-data provider from the
previously qualified `4abc99b2cf8ef298d9c798a13331afdcdfae37b1` adapter. It
keeps external acquisition, cancellation and provenance available to the
preview API, Python SDK and MCP tools. The ordinary working xGRIB installation
and its source branch are not replaced by this adapter variant.

Routing now includes provider-supported gusts, including forecast extension,
size estimates and cache validation. Existing Routing GRIBs need regeneration.
AIFS has no published gust field. Hosted GFS gust-aware failover requires the
matching service 0.2.2 and its five-field weather cache; old service responses
are rejected rather than silently losing gusts.

Both architectures must pass xGRIB's complete tests and the bundle's clean
installation, provider registration and API smoke checks before publication.
The historical `xgrib-0.3.0.0-provider.patch` remains a reference for the older
assembly only; it is not applied to this pinned preview source.
