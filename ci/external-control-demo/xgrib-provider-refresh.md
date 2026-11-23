# xGRIB 0.4.6 developer-bundle integration

The bundle pins preview xGRIB `d539649dd416875aae36a8da555a883e43990972` (version 0.4.6), based on
the fully qualified production release `c5a05468cd55061a50009f7a8ddf7657082a020b`.
Generator 0.4.0 remains pinned at `4208c87dbf223bedb78064043313c5fc09c629f1`.

The environmental provider and file-digest implementation retain the previously
qualified 0.4.5 preview behaviour from `5653f9ab1f5bd76fca1058d47171a5132a6741f1`.
Four UTF-8 conversions now pass explicit data pointers and lengths, supporting
older wxWidgets on Android while preserving string contents. The wire contract
checks these conversions. Registration/acquisition/cancellation/provenance and
the API 1.21 loading floor are retained. The normal GUI works on a stock host without the
optional preview API.

The production base adds native CCSDS-compressed GRIB2 decoding (template 42)
and retains external-forecast display, gusts and NWS time-range validation.
Both bundle architectures must pass the full plugin tests, clean installation,
provider registration and API/MCP smoke checks. Hosted GFS fallback retains its
service 0.2.2/five-field-cache prerequisite; no service deployment is included.
