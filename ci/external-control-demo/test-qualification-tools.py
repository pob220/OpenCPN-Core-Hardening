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


class FlatpakManifest(unittest.TestCase):
    def test_isolation_pins_and_explicit_tests(self):
        with tempfile.TemporaryDirectory(prefix="preview-manifest-test-") as work:
            original = Path(work) / "org.opencpn.OpenCPN.yaml"
            original.write_text(json.dumps({
                "app-id": "org.opencpn.OpenCPN", "add-extensions": {"stock": {}},
                "finish-args": ["--filesystem=home"],
                "modules": [{"name": "opencpn", "config-opts": ["-DOCPN_RELEASE=1"],
                             "sources": [{"type": "git", "url": "https://example.invalid"}]}],
            }))
            revision = "a" * 40
            result = subprocess.run(
                [sys.executable, str(HERE / "prepare-flatpak-probe.py"), str(original), revision],
                capture_output=True, text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            manifest = json.loads(Path(result.stdout.strip()).read_text())
            self.assertEqual(manifest["app-id"], "io.github.pob220.OpenCPNCorePreview")
            self.assertNotIn("add-extensions", manifest)
            self.assertFalse(any("filesystem" in arg for arg in manifest["finish-args"]))
            core = manifest["modules"][-1]
            self.assertEqual(core["sources"][0]["commit"], revision)
            self.assertTrue(core["run-tests"])
            self.assertEqual(core["test-rule"], "")
            self.assertEqual(len(core["test-commands"]), 2)
            self.assertIn("--gtest_output=xml:", core["test-commands"][0])
            self.assertIn("verify-test-report.py", core["test-commands"][1])
            self.assertEqual([m["name"] for m in manifest["modules"][:-1]],
                             ["nlohmann-json", "googletest"])
            for dependency in manifest["modules"][:-1]:
                self.assertRegex(dependency["sources"][0]["commit"], r"^[0-9a-f]{40}$")


class DependencyPatching(unittest.TestCase):
    def test_patch_reconfigure_and_failure(self):
        source_root = HERE.parent.parent
        for dependency in ("shapelib", "rapidjson"):
            with self.subTest(dependency=dependency), tempfile.TemporaryDirectory(
                prefix="preview-patch-test-"
            ) as work:
                source = Path(work) / "sample.txt"
                source.write_text("before\n")
                patch = Path(work) / "change.patch"
                patch.write_text(
                    "--- a/sample.txt\n+++ b/sample.txt\n@@ -1 +1 @@\n-before\n+after\n"
                )
                command = ["cmake", f"-Dpatch_file={patch}", f"-Dpatch_dir={work}",
                           "-P", str(source_root / "libs" / dependency / "cmake/PatchFile.cmake")]
                for _ in range(2):
                    result = subprocess.run(command, capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertEqual(source.read_text(), "after\n")
                source.write_text("incompatible source\n")
                result = subprocess.run(command, capture_output=True)
                self.assertNotEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
