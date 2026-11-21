# Chart cache follow-up for Weather Routing 1.41.0 and 2.8.0

Both plugin releases use this companion OpenCPN core correction. Their versions, plugin APIs, routing algorithms, candidate ordering and route validation rules remain unchanged.

## Persistent certificate round trips

The wxJSON writer emits integral doubles as integer JSON tokens. Its `AsDouble()` accessor reads the double union member without converting integer values. Loading certificates with whole-number coordinates or depths could therefore introduce tiny invalid coordinates or NaNs, and a subsequent save could produce invalid JSON.

Read numeric tokens according to their actual type, convert integers explicitly, and require finite, canonical coordinates and valid policy metadata before admitting a certificate. Validate the key against its chart identity, group, region and policy. Reject invalid entries when loading, storing and saving. An empty cache serializes as an array.

The derived-certificate file format changes from 3 to 4 and records the chart catalogue identity. Derived proofs lack individual chart dependencies, so files from older versions, changed catalogues or unknown contexts are discarded and regenerated from authoritative chart data. Do not repair a NaN file by treating its contents as proofs. Raw semantic tiles retain their existing dependency checks and format; the installed global starting layer is unchanged.

## Loading chart objects

Appending each CM93 object previously walked its entire display-priority/style linked list. In a loaded chart context, repeated walks dominated sampled native tile preparation. `LoadViewportCells` now owns stack-scoped append cursors, one per priority/style, shared across the cells and subcells loaded in that synchronous call. Each existing chain is traversed once per batch; subsequent insertions append at its tail. Ordinary intervening appends retain their place. New batches and chart owners start with fresh cursors. No cursor survives chart mutation or destruction.

This preserves the original natural object order and all geometry, coverage, provider precedence, hazards, depth evidence and final safety checks. It adds no threads or persistent chart index.

## Controlled replay

The baseline is companion core `a05ecaba8cdc569364859d7eb75a4110ee48f6a1`, used by the previous 1.41.0/2.8.0 qualification. A private diagnostic host clears semantic tile results between requests while retaining native chart state. Both hosts receive the same 40 tile requests, interleaved with the same seeded sequence of 4,096 positive regions from the refined global starting layer. This is a distributed warming sample, not a complete global import or whole-passage benchmark.

| Measurement | Baseline | Fixed |
|---|---:|---:|
| First cold tile batch | 7.059 s | 2.419 s |
| Warming 4,096 regions, cumulative | 286.671 s | 119.041 s |
| Repeated tile batch after warming | 2.300 s | 2.324 s |

All ten exported responses are byte-identical between baseline and fixed hosts, including the initial cold response. Repeated queries still reflect loaded-chart scanning costs. These measurements support a 58.5% reduction in this chart-loading workload, not a 58.5% whole-route speedup.

Qualification artifacts and platform receipts are recorded separately in `output/chart-cache-followup-20261121`. The private replay hook is not part of the shipping core.
