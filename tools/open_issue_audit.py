#!/usr/bin/env python3
"""Create the reproducible issue appendix used by the architecture audit.

Input is the JSON array returned by GitHub's ``/repos/OpenCPN/OpenCPN/issues``
endpoint after pull requests have been removed.  The broad classification is
deliberately conservative and title/label based; issue-specific conclusions
which were verified in code are recorded in OVERRIDES.  Rows which have not
received a code-level review say so explicitly instead of pretending that an
automated taxonomy is a diagnosis.
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import re
from pathlib import Path


AUDIT_DATE = dt.date(2026, 8, 18)


# issue: subsystem, kind, priority, confidence, reproduction, HEAD status,
# related issues/PRs, source areas, root-cause assessment
OVERRIDES = {
    5364: ("charts/S63", "defect", "B", "medium", "reported deterministic with six cells", "new/open; root cause not established", "#4106; #4119; #2178", "gui/src/ocpn_frame.cpp; chart catalogue; S63 plug-in boundary", "Chart rebuild can hang or OOM after catalogue corruption, but successful standalone OCPNsenc means the failing core/plug-in orchestration still needs tracing."),
    5311: ("communications/navigation", "defect", "A", "proven", "deterministic", "open on upstream master", "", "model/src/autopilot_output.cpp", "Proven unit mismatch: knots passed where N2K requires m/s."),
    5306: ("startup/rendering", "defect", "B", "medium", "reported deterministic on one Windows system", "open; symbolized stack still needed", "#5296; #5231", "gui/src/gl_chart_canvas.cpp; plug-in render callbacks", "Null read is proven by the dump, but the final log line does not establish whether core rendering, the graphics driver, or an o-charts callback supplied the null object."),
    5304: ("navigation/UI", "defect", "C", "high", "reported deterministic", "open on upstream master", "", "gui/src/chcanv.cpp; gui/src/ocpn_frame.cpp", "Dateline midpoint is computed by arithmetic averaging; an asynchronous WMM plug-in query also returns global state rather than a value associated with the request."),
    5287: ("startup/UI", "defect", "B", "proven", "first-run deterministic", "candidate fix on upstream master; regression/backport verification needed", "", "gui/src/ocpn_frame.cpp; gui/src/toolbar.cpp", "Release 5.14 permits an event before g_MainToolbar exists and dereferences it. Current master routes this through null-safe HideTbarTooltip, but has no issue-linked regression proof."),
    5240: ("Android/release support", "compound defect report", "C", "low", "multiple symptoms on one device", "needs issues split/minimized", "", "Android packaging/UI/plugins (symptom-specific)", "Umbrella report is useful release feedback but cannot have one code root cause; split into reproducible defects."),
    5170: ("charts/plugins", "defect", "B", "high", "intermittent", "open on upstream master", "", "gui/src/ocpn_plugin_gui.cpp; gui/src/chartdbs.cpp", "AddChartToDBInPlace deletes and recreates global ChartData while canvases and plug-ins can retain references."),
    5145: ("AIS/Signal K", "defect", "B", "proven", "input-dependent", "open on upstream master", "", "model/src/ais_decoder.cpp", "RapidJSON values are read using GetString/GetBool/GetInt without type checks; malformed or schema-drifted input asserts or has undefined behaviour."),
    5144: ("routes/storage", "defect", "B", "proven", "deterministic upgrade", "fixed on upstream master; release verification needed", "#5200; #5202", "model/src/navobj_db_migrator.cpp; model/src/navobj_db.cpp", "A database schema change shipped without the required migration; master now contains the migration."),
    5200: ("routes/storage", "defect", "B", "proven", "deterministic", "open on upstream master", "#5202; #5144", "model/src/navobj_db.cpp; gui/src/navutil.cpp", "Route deletion removes relationship rows but leaves route-only points orphaned; existing orphan cleanup is unused and would also need to preserve isolated marks."),
    5202: ("routes/storage", "defect", "B", "proven", "deterministic", "open on upstream master", "#5200; #5144", "model/src/navobj_db.cpp; model/src/route.cpp", "Same persistence/ownership defect as #5200, made visible when shared waypoints are involved."),
    5102: ("charts/performance", "defect", "E", "high", "reported deterministic at scale", "open on upstream master", "", "gui/src/mbtiles; gui/src/chartdb.cpp", "Large-catalog path needs profiling; likely synchronous catalogue/tile work, not yet proven."),
    5069: ("AIS", "defect/compliance", "A", "medium", "supplied message corpus", "open on upstream master", "", "model/src/ais_decoder.cpp; model/src/ais_target_data.cpp", "Type-14 lifetime and presentation are attached to target state; the claimed standards interpretation requires maintainer/spec verification before behaviour changes."),
    4983: ("charts/rendering", "defect", "B", "high", "intermittent under cache rebuild", "open on upstream master", "#3851; #4842", "gui/src/gl_texture_mgr.cpp; gui/src/gl_tex_cache.cpp", "Strong lifetime/race hypothesis around asynchronous texture-cache jobs; exact crashing interleaving not yet proven."),
    5296: ("charts/plugins", "defect", "B", "low", "intermittent and scope changed during thread", "open on upstream master", "#5248; #5231; #5306", "gui/src/chartdbs.cpp; gui/src/pluginmanager.cpp; o-charts plug-in state", "Later reports point toward persistent o-charts/EULA state, but the issue's original claim that plug-ins and chart type were excluded was not reproduced."),
    5248: ("charts/catalogue", "defect", "C", "low", "not reproduced after directory reset", "open; likely support/corrupt-state case", "#5296", "gui/src/chartdbs.cpp; chartlist.dat", "Deleting and re-adding chart directories reportedly restored operation; no code cause established."),
    5231: ("charts/rendering/plugins", "defect", "B", "medium", "hardware/data-set dependent", "open on upstream master", "#5296; #5306", "gui/src/gl_*; o-charts plug-in; Windows graphics backend", "Long thread narrows part of the problem to Intel Arc/Windows/plug-in interaction, but does not prove one core defect."),
    4865: ("charts/CM93", "defect", "B", "low", "unconfirmed", "open on upstream master", "", "gui/src/cm93.cpp", "Crash report lacks a minimized chart/input and code-level cause."),
    4843: ("routes/plugins/storage", "defect", "C", "medium", "platform-specific recipe", "open on upstream master", "", "gui/src/api_121.cpp; model/src/navobj_db.cpp", "Plug-in update path duplicates delete/re-add/index/persistence operations; a stale-instance or ordering defect is plausible but unproven."),
    4842: ("charts/rendering", "defect", "B", "medium", "intermittent macOS", "possibly affected by #4914; needs closure verification", "#4983; #3851; PR #4914", "gui/src/gl_texture_mgr.cpp; gui/src/gl_tex_cache.cpp", "Texture lifetime issue; later shared-ownership work may have fixed it, but no regression proof is attached."),
    4745: ("tides/UI", "defect", "F", "high", "reported deterministic", "crash portion fixed; formatting remains", "", "gui/src/tc_win.cpp", "Current open scope is panel formatting, not the already-fixed crash mentioned in the body."),
    4780: ("communications/charts", "defect", "C", "medium", "reported deterministic", "open on upstream master", "", "gui/src/chartdldr; model/src/comm_drv_n0183_net.cpp", "Likely GUI/main-loop starvation during chart download; needs a trace before extraction."),
    4686: ("communications/N2K", "defect", "A", "medium", "hardware-dependent", "open on upstream master", "#4554; #3316; #5311", "model/src/autopilot_output.cpp; model/src/comm_drv_n2k_serial.cpp", "N2K output/serial cluster; exact crash cause not proven."),
    4670: ("AIS/ownship", "defect/robustness", "A", "high", "deterministic with device input", "open on upstream master", "", "model/src/comm_bridge.cpp; model/src/ais_decoder.cpp", "External device emits another target as AIVDO; core accepts it as ownship without an MMSI/configuration consistency policy."),
    4554: ("communications/N2K", "defect", "B", "high", "intermittent hardware-dependent", "open on upstream master", "#3316; #4686", "model/src/comm_drv_n2k_serial.cpp; model/src/comm_drv_registry.cpp", "Serial-driver fault and shutdown crash are reported. Plain cross-thread flags, raw parent/thread pointers and non-joining Close are concrete hazards; causal interleaving remains unproven."),
    4432: ("routes/UI", "defect", "J", "proven resolved", "deterministic", "fixed by PR #4434; close after verification", "PR #4434", "gui/src/route_prop_dlg.cpp", "Issue carries done label and linked fix."),
    4642: ("startup/build", "defect", "J", "low", "old 5.11 beta", "likely stale; revalidate current release", "", "startup/build packaging", "Unable-to-start report targets obsolete beta builds and needs a current reproduction."),
    4357: ("charts/rendering", "defect", "J", "low", "old macOS 5.10.2", "likely stale; revalidate current release", "", "gui/src/gl_chart_canvas.cpp", "Old zoom crash without a current symbolized reproduction."),
    4103: ("route activation/autopilot", "defect", "A", "medium", "at-sea/hardware-dependent", "open on upstream master", "", "model/src/routeman.cpp; model/src/autopilot_output.cpp", "Transition-time XTE/direction semantics may drive an autopilot turn the wrong way; no proven root cause. Requires sentence-sequence characterization before a fix."),
    4010: ("communications/N2K", "defect", "B", "low", "unreproduced", "open on upstream master", "#4554; #3316", "model/src/comm_drv_n2k_net.cpp", "Old macOS N2K TCP hang without a current reproduction."),
    3934: ("platform/rendering", "defect", "C", "medium", "display-scale change", "open on upstream master", "", "gui/src/viewport.cpp; platform DPI/display events", "Viewport coordinate transform is corrupted in memory after a Gnome scale change; this is not persistent data corruption."),
    3866: ("shutdown/plugins", "defect/performance", "B", "medium", "intermittent", "open on upstream master", "#4554", "gui/src/ocpn_frame.cpp; gui/src/pluginmanager.cpp", "Long procedural shutdown and plug-in/device ordering are plausible contributors; no single cause proven."),
    3851: ("charts/S57", "defect", "B", "proven by maintainer analysis", "load/cache-pressure dependent", "master has substantial candidate lifecycle hardening; regression/closure verification needed", "#4983; #4842", "gui/src/senc_manager.cpp; gui/src/s57chart.cpp; gui/src/chartdb.cpp", "Original cache purge could delete a chart associated with SENC work. Current master adds invalidation, completion tracking, locking and shutdown handling, but detached workers and bounded-wait ticket deletion still require deterministic lifetime review."),
    3570: ("charts/S57", "defect/robustness", "C", "proven input problem", "deterministic malformed update", "open on upstream master", "", "gui/src/s57chart.cpp; gui/src/senc_manager.cpp", "Official ENC update input is invalid; OpenCPN's repeated retry and misleading recovery are the actionable defects."),
    3412: ("tracks/storage", "defect", "J", "high", "crash recovery", "marked done; fix commit 84f2b70bf needs reporter verification", "commit 84f2b70bf", "model/src/navobj_db.cpp; model/src/track.cpp", "Active-track crash recovery was reportedly implemented."),
    3316: ("communications/concurrency", "defect", "B", "medium", "hardware-dependent", "open on upstream master", "#4554; #4686", "model/src/comm_drv_n2k_serial.cpp; model/src/comm_drv_n0183_net.cpp", "Concurrent N2K serial/N0183 network crash belongs to the same driver lifetime/input cluster; exact cause unproven."),
    3190: ("routes/storage", "defect", "B", "medium", "import-dependent", "open on upstream master", "#3189", "model/src/navobj_db.cpp; gui/src/navutil.cpp", "GUID uniqueness and import merge policy are not enforced at one transactional boundary."),
    3189: ("routes/storage", "defect", "C", "medium", "import-dependent", "open on upstream master", "#3190", "model/src/navobj_db.cpp; gui/src/navutil.cpp", "Duplicate-GUID/import invariant cluster."),
    1961: ("navigation/plugins", "defect/semantics", "C", "high", "watchdog deterministic", "behaviour changed: master sets zero on timeout", "", "model/src/comm_bridge.cpp; gui/src/ocpn_frame.cpp; include/ocpn_plugin.h", "Current code explicitly invalidates FixTime to zero while plug-in documentation says system time after timeout; contract and implementation disagree."),
    2603: ("Android/developer tooling", "feature/enhancement", "H", "high as request", "not applicable", "open design request", "", "Android build/debug tooling", "A developer-debugging capability request, not evidence of a production crash."),
    2493: ("startup/UI", "defect", "B", "medium", "startup timing dependent", "old but plausibly related to #5287", "#5287", "gui/src/ocpn_frame.cpp", "Settings was available before canvas/application initialization completed; retain as a historical reproduction for the readiness-state fix."),
    1614: ("plugin management", "defect", "C", "high", "deterministic malformed XML", "old; current plug-in manager needs revalidation", "", "gui/src/pluginmanager.cpp; model/src/catalog_parser.cpp", "Malformed catalogue error handling entered a hang in the historical implementation; retain as an input corpus, not as proof against current HEAD."),
    1325: ("charts/rendering", "defect", "J", "low", "2018 multi-canvas crash", "likely stale; revalidate before work", "", "gui/src/chcanv.cpp", "Old crash against substantially changed multi-canvas code; no current reproduction."),
    100: ("cross-cutting", "architecture/debt", "G", "high as debt, not as defect", "not applicable", "open design task", "", "multiple historical raw-pointer owners", "Smart pointers should be introduced only while clarifying a demonstrated ownership defect; a global conversion campaign is not justified."),
}


DEEP_CODE = {
    5311, 5304, 5287, 5202, 5200, 5170, 5145, 5144, 5069, 4983, 4843,
    4842, 4686, 4670, 4554, 4432, 4103, 4010, 3866, 3851, 3570, 3412,
    3316, 3190, 3189, 1961,
}


def has(text: str, *words: str) -> bool:
    return any(word in text for word in words)


def classify(issue: dict) -> tuple[str, str, str]:
    text = (issue.get("title", "") + " " + issue.get("body", "")).lower()
    labels = {item["name"].lower() for item in issue.get("labels", [])}

    if has(text, "ais", "aivdm", "aivdo", "mmsi", "cpa", "sart"):
        subsystem = "AIS"
    elif has(text, "route", "waypoint", "track", "gpx", "navobj"):
        subsystem = "routes/tracks/storage"
    elif has(text, "chart", "s57", "s-57", "s63", "s-63", "senc", "enc ", "mbtiles", "cm93", "quilt"):
        subsystem = "charts/rendering"
    elif has(text, "nmea", "n2k", "signal k", "signalk", "tcp", "udp", "serial", "gps", "autopilot", "connection"):
        subsystem = "communications/navigation"
    elif has(text, "plugin", "plug-in", "api "):
        subsystem = "plugins"
    elif has(text, "tide", "current station", "harmonic"):
        subsystem = "tides/currents"
    elif has(text, "android", "macos", "windows", "wayland", "flatpak", "build", "compile", "cmake"):
        subsystem = "platform/build"
    elif has(text, "startup", "start-up", "shutdown", "exit", "first run"):
        subsystem = "startup/shutdown"
    else:
        subsystem = "UI/general"

    defect_words = ("crash", "hang", "freeze", "wrong", "incorrect", "lost", "loss", "fail", "broken", "does not", "doesn't", "unable", "regression", "corrupt", "segfault", "exception")
    if "enhancement" in labels and "bug" not in labels:
        kind = "feature/enhancement"
    elif "bug" in labels or has(text, *defect_words):
        kind = "defect"
    elif has(text, "documentation", "manual", "wiki"):
        kind = "documentation/support"
    elif has(text, "build", "compile", "package", "flatpak"):
        kind = "build/release"
    elif "enhancement" in labels:
        kind = "feature/enhancement"
    else:
        kind = "needs triage"

    if "done" in labels:
        priority = "J"
    elif kind == "feature/enhancement":
        priority = "H"
    elif kind == "documentation/support":
        priority = "I"
    elif kind == "build/release":
        priority = "D"
    elif has(text, "crash", "segfault", "corrupt", "data loss", "lost route", "lost track"):
        priority = "B"
    elif has(text, "wrong course", "wrong bearing", "autopilot", "position jump", "teleport", "safety"):
        priority = "A"
    elif has(text, "slow", "performance", "lag", "cpu", "memory"):
        priority = "E"
    elif kind == "defect":
        priority = "C"
    else:
        priority = "F"
    return subsystem, kind, priority


def platform(issue: dict) -> str:
    text = (issue.get("title", "") + " " + issue.get("body", "")).lower()
    values = []
    for word, label in (("android", "Android"), ("macos", "macOS"), ("mac os", "macOS"), ("windows", "Windows"), ("linux", "Linux"), ("raspberry", "Raspberry Pi"), ("wayland", "Wayland"), ("flatpak", "Flatpak")):
        if word in text and label not in values:
            values.append(label)
    return "|".join(values) if values else "not established/cross-platform"


def source_hint(subsystem: str) -> str:
    return {
        "AIS": "model/src/ais_decoder.cpp; gui/src/ais.cpp",
        "routes/tracks/storage": "model/src/{route,track,navobj_db}.cpp; gui/src/navutil.cpp",
        "charts/rendering": "gui/src/{chartdbs,chartimg,gl_*}.cpp",
        "communications/navigation": "model/src/comm_*; model/src/georef.cpp",
        "plugins": "include/ocpn_plugin.h; gui/src/{pluginmanager,ocpn_plugin_gui,api_121}.cpp",
        "tides/currents": "gui/src/tcmgr.cpp; gui/src/tcwin.cpp",
        "platform/build": "CMakeLists.txt; cmake/; gui/src/ocpn_platform.cpp",
        "startup/shutdown": "gui/src/{ocpn_app,ocpn_frame}.cpp",
        "UI/general": "gui/src (issue-specific)",
    }[subsystem]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    issues = json.loads(args.input.read_text())
    issues = [i for i in issues if "pull_request" not in i]
    issues.sort(key=lambda i: i["number"], reverse=True)

    fields = ["issue", "title", "opened", "age_years", "labels", "platform", "subsystem", "kind", "priority_class", "severity", "validity_confidence", "reproducibility", "head_status", "related", "source_areas", "root_cause_assessment", "audit_depth"]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=fields)
        writer.writeheader()
        for issue in issues:
            opened = dt.date.fromisoformat(issue["created_at"][:10])
            labels = "|".join(item["name"] for item in issue.get("labels", []))
            subsystem, kind, priority = classify(issue)
            severity = {"A": "safety/correctness critical", "B": "crash/hang/data integrity", "C": "serious functional defect", "D": "platform/build/release", "E": "performance/resource", "F": "usability/UI", "G": "architecture/debt", "H": "feature request", "I": "documentation/support", "J": "likely resolved/stale/duplicate"}[priority]
            confidence = "medium" if issue.get("comments", 0) else "low"
            reproduction = "not established"
            status = "not code-verified"
            related = ""
            sources = source_hint(subsystem)
            root = "Unresolved; taxonomy-only triage. Read/reproduce before changing code."
            depth = "metadata/title/body triage"
            if issue["number"] in OVERRIDES:
                subsystem, kind, priority, confidence, reproduction, status, related, sources, root = OVERRIDES[issue["number"]]
                severity = {"A": "safety/correctness critical", "B": "crash/hang/data integrity", "C": "serious functional defect", "D": "platform/build/release", "E": "performance/resource", "F": "usability/UI", "G": "architecture/debt", "H": "feature request", "I": "documentation/support", "J": "likely resolved/stale/duplicate"}[priority]
                depth = ("thread and code reviewed" if issue["number"] in DEEP_CODE
                         else "manual issue review")
            writer.writerow({"issue": issue["number"], "title": re.sub(r"\s+", " ", issue["title"]).strip(), "opened": opened.isoformat(), "age_years": f"{(AUDIT_DATE-opened).days/365.25:.1f}", "labels": labels, "platform": platform(issue), "subsystem": subsystem, "kind": kind, "priority_class": priority, "severity": severity, "validity_confidence": confidence, "reproducibility": reproduction, "head_status": status, "related": related, "source_areas": sources, "root_cause_assessment": root, "audit_depth": depth})

    if len(issues) != 319:
        raise SystemExit(f"Expected 319 open issues at audit snapshot, got {len(issues)}")


if __name__ == "__main__":
    main()
