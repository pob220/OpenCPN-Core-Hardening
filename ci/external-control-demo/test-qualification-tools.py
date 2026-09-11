#!/usr/bin/env python3
"""Offline regression checks for the platform qualification safeguards."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

HERE = Path(__file__).resolve().parent


class TestReportValidation(unittest.TestCase):
    def check_report(self, body, success):
        with tempfile.TemporaryDirectory(prefix="preview-report-test-") as work:
            report = Path(work) / "tests.xml"
            report.write_text(f"<testsuites><testsuite>{body}</testsuite></testsuites>")
            result = subprocess.run(
                [sys.executable, str(HERE / "verify-test-report.py"), str(report)],
                capture_output=True, text=True,
            )
            self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)

    @staticmethod
    def cases(status="run", failure=""):
        return "".join(
            f'<testcase classname="{suite}" name="example" status="{status}">{failure}</testcase>'
            for suite in ("ExternalApiTest", "ChartSafetyDepth", "ChartSafetyService")
        )

    def test_successful_report(self):
        self.check_report(self.cases(), True)

    def test_zero_tests_rejected(self):
        self.check_report("", False)

    def test_failure_rejected(self):
        self.check_report(self.cases(failure="<failure/>"), False)

    def test_notrun_rejected(self):
        self.check_report(self.cases(status="notrun"), False)

    def test_skipped_rejected(self):
        self.check_report(self.cases(failure="<skipped/>"), False)

    def test_missing_suite_rejected(self):
        self.check_report('<testcase classname="ExternalApiTest" name="only"/>', False)

    def test_missing_report_rejected(self):
        with tempfile.TemporaryDirectory(prefix="preview-report-test-") as work:
            result = subprocess.run(
                [sys.executable, str(HERE / "verify-test-report.py"), str(Path(work) / "missing.xml")],
                capture_output=True,
            )
            self.assertNotEqual(result.returncode, 0)


class PlatformMatrix(unittest.TestCase):
    def test_native_linux_target_identities(self):
        targets = json.loads((HERE / "linux-platforms.json").read_text())
        self.assertEqual(len(targets), 4)
        self.assertEqual(len({t["platform"] for t in targets}), 4)
        for target in targets:
            self.assertIn(target["arch"], ("x86_64", "aarch64"))
            self.assertEqual("arm" in target["runner"], target["arch"] == "aarch64")
            self.assertGreater(target["parallel"], 0)


if __name__ == "__main__":
    unittest.main()
