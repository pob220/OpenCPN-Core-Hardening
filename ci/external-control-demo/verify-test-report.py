#!/usr/bin/env python3
"""A successful process alone does not prove that qualification tests ran."""
import sys
import xml.etree.ElementTree as ET

root = ET.parse(sys.argv[1]).getroot()
cases = list(root.iter("testcase"))
executed = [case for case in cases if case.find("skipped") is None
            and case.get("status") != "notrun"]
if not executed:
    raise SystemExit("Qualification report contains no executed tests")
if any(case.find("failure") is not None or case.find("error") is not None for case in cases):
    raise SystemExit("Qualification report contains test failures")
for suite in ("ExternalApiTest", "ChartSafetyDepth", "ChartSafetyService"):
    if not any(case.get("classname") == suite for case in executed):
        raise SystemExit(f"Required test suite did not execute: {suite}")
print(f"Verified {len(executed)} executed qualification tests with no failures")
