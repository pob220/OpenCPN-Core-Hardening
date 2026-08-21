# External-control Preview C scheduler contract

Status: implementation contract, 2026-08-21

## Purpose

Preview C demonstrates that an independently running client can discover and
compose typed OpenCPN services without automating dialogs or depending on
plugin implementation details.  It adds two workflows:

1. acquire, validate, publish and display an environmental dataset; and
2. acquire a dataset and calculate a chart/depth-aware weather route using the
   exact immutable dataset produced by the first step.

The scheduler is a client.  It does not run inside OpenCPN, own navigation
state, parse GRIB, calculate routes, or invoke arbitrary plugin messages.

## Compatibility boundary

The existing plugin class ABI remains API 1.21.  Preview providers register
through optional C entry points resolved at runtime.  A plugin loaded by a host
without these entry points retains its ordinary GUI behaviour and omits the
external capability.

Preview work lives on explicit feature branches.  It does not introduce
duplicate xxGRIB or xxWeatherRouting plugin identities: those would collide
with existing GRIB messages/configuration and create divergent engines.

## Provider discovery

`GET /api/v2/providers` returns descriptors visible to the authenticated
principal.  Descriptors use ordinary typed fields internally and contain:

- stable capability, display name, provider kind and schema version;
- cancellability and concurrency policy;
- typed fields with units, bounds, defaults and enumerated/resource values;
- resource catalogues such as weather providers, presets and polar identities;
- required scopes and non-secret credential-availability status.

OpenCPN exposes only providers explicitly registered for this contract.  It
does not reflect arbitrary plugin preferences or wxWidgets controls.

## Environmental datasets

Acquisition is a bounded asynchronous job.  A successful provider result is an
immutable dataset reference with provider/model/cycle, geographic coverage,
valid time range, fields, checksum, byte size and provenance.  Provider-private
file paths and handles are not serialized by the external API.

A dataset is published only after strict validation.  Publication and display
activation are distinct operations.  Failure, cancellation or partial output
must leave the active dataset unchanged.  A planning job pins the exact
dataset identity supplied in its request even if another dataset is activated.

## Scheduling semantics

Schedule definitions conform to `api/scheduler-schedule-v1.schema.json`.
Triggers initially support interval, UTC cron and explicit manual execution.
Each schedule declares a concurrency policy (`skip`, `queue` or `replace`), a
missed-run policy, maximum runtime and bounded retry/backoff.

Spatial start and departure time are independent:

- start location is fixed or a fresh ownship snapshot captured at execution;
- departure time is fixed UTC, current time plus an offset, or the acquired
  dataset start plus an offset.

An ownship start fails closed when position is invalid or older than the
schedule's configured maximum age.

## Route publication policy

Planning results are never activated for navigation.  After OpenCPN performs
an independent authoritative chart-safety validation, the scheduler creates or
transactionally replaces only the draft it owns.  A failed calculation leaves
the last known-good draft in place.  The Preview C credential intentionally
lacks `routes:activate`.

## Events

The bounded semantic stream adds environmental job, dataset publication and
dataset activation events.  Events contain identities and state transitions,
not GRIB records, filesystem paths or credentials.  Sequence gaps retain the
existing explicit resynchronisation semantics.

## Acceptance invariants

- malformed provider descriptors and requests are rejected before mutation;
- jobs are cancellable and callbacks cannot outlive an unloaded provider;
- only fully validated immutable datasets can become active;
- exact dataset provenance reaches the planning result and schedule history;
- stale ownship, unknown chart depth and failed chart validation fail closed;
- external plans remain drafts and never produce autopilot/NMEA output;
- stock OpenCPN behaviour of xGRIB and xWeatherRouting remains unchanged;
- restart, provider unload and interrupted-download tests retain the previous
  last known-good dataset and route draft.
