# OpenCPN External Control Demo

The External Control Demo is the single developer entry point for the
authenticated `/api/v2` work. It combines the hardened OpenCPN 5.15 core,
xGRIB, xWeatherRouting and the independent Scheduler without merging their
source repositories or changing native plug-in API 1.21.

Release and one-package installation:

<https://github.com/pob220/OpenCPN-Core-Hardening/releases/tag/external-control-demo-20260821>

The release archive contains exact component revisions, checksums, offline
Python wheels, resource-complete plug-ins, generic launchers, wire schemas and
an isolated installer. The installer refuses to overwrite a non-empty
directory, generates a least-privilege local token, uses a separate OpenCPN
configuration and does not require `sudo`.

The demonstration exercises two useful vertical slices:

1. scheduled xGRIB environmental acquisition followed by activation in the
   running OpenCPN display; and
2. acquisition followed by xWeatherRouting using the exact immutable dataset,
   with independent core chart/land/depth validation before publishing a
   non-active draft.

## Moving-vessel scheduled replanning example

The combined workflow can be configured to run at an anchored four-hour
interval with `ownship` as its route start. Each occurrence:

1. acquires and validates a new xGRIB environmental dataset;
2. activates that exact dataset in the running OpenCPN display;
3. samples OpenCPN's current ownship position immediately before planning;
4. asks xWeatherRouting for a route from that position to the configured
   destination; and
5. publishes the result only after authoritative core chart/land/depth
   validation succeeds.

Ownship position has a configurable maximum age (30 seconds is a useful test
value). Missing, invalid or stale navigation data fails the route step
explicitly. Acquisition, planning or safety failure does not replace the
schedule's last known-good draft. A successful run transactionally replaces
only that schedule-owned, non-active draft; it does not activate navigation,
control the vessel or send autopilot output.

The current xGRIB acquisition area is a fixed bounding box, not an
automatically moving corridor. Testers must configure it to cover the moving
boat-to-destination route. Automatic area derivation is deliberately deferred
contract work rather than hidden plug-in UI automation.

It is a Linux x86_64 developer demonstration, not a navigation release. The
release page and bundled qualification document state the tested platform,
evidence and limitations. Existing onboard profiles and installations must
not be used as the test target.
