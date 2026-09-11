#!/usr/bin/env python3
"""Generate a core-only diagnostic manifest, never overwrite the stable app ID."""
import json
import re
import sys
from pathlib import Path

import yaml

manifest_path = Path(sys.argv[1])
core_revision = sys.argv[2]
if not re.fullmatch(r"[0-9a-f]{40}", core_revision):
    raise SystemExit("Expected an exact 40-character core commit")
manifest = yaml.safe_load(manifest_path.read_text())
assert manifest["app-id"] == "org.opencpn.OpenCPN"
manifest["app-id"] = "io.github.pob220.OpenCPNCorePreview"
manifest.pop("add-extensions", None)
# No host profile, device, or system-bus access for the core-only probe.
manifest["finish-args"] = ["--socket=x11", "--share=ipc", "--share=network", "--device=dri"]
modules = manifest["modules"]
core = next(module for module in modules if module["name"] == "opencpn")
assert core["sources"][0]["type"] == "git"
core["sources"][0] = {
    "type": "git",
    "url": "https://github.com/pob220/OpenCPN-Core-Hardening.git",
    "commit": core_revision,
}
core["sources"].append({
    "type": "file",
    "path": "../../ci/external-control-demo/verify-test-report.py",
    "dest": "preview-ci",
})
core["config-opts"] = [
    option for option in core["config-opts"] if not option.startswith("-DOCPN_RELEASE=")
] + ["-DOCPN_RELEASE=0", "-DOCPN_BUILD_TEST=ON", "-DOCPN_USE_GL=ON",
     "-DOCPN_USE_VULKAN_PRESENTER=OFF"]
# Supply GoogleTest as a hashed git source before entering the offline build.
modules.insert(modules.index(core), {
    "name": "nlohmann-json",
    "buildsystem": "cmake-ninja",
    "config-opts": ["-DJSON_BuildTests=OFF", "-DJSON_Install=ON"],
    "sources": [{"type": "git", "url": "https://github.com/nlohmann/json.git",
                 "commit": "55f93686c01528224f448c19128836e7df245f72"}],
})
modules.insert(modules.index(core), {
    "name": "googletest",
    "buildsystem": "cmake-ninja",
    "config-opts": ["-DBUILD_GMOCK=OFF", "-DINSTALL_GTEST=ON"],
    "sources": [{"type": "git", "url": "https://github.com/google/googletest.git",
                 "commit": "58d77fa8070e8cec2dc1ed015d66b454c8d78850"}],
})
core["run-tests"] = True
# Flatpak defaults to `make check`; OpenCPN uses explicit GoogleTest commands.
# Disable only the implicit target, not the test phase or report validation.
core["test-rule"] = ""
core["test-commands"] = [
    "test/tests --gtest_filter=ExternalApiTest.*:InProcessPlanningJobServiceTest.*:"
    "BoundedApplicationEventStreamTest.*:ChartSafetyDepth.*:ChartSafetyService.* "
    "--gtest_output=xml:preview-tests.xml",
    "python3 ../preview-ci/verify-test-report.py preview-tests.xml",
]
# Keep paths relative to the pinned upstream manifest directory.
output = manifest_path.with_name("io.github.pob220.OpenCPNCorePreview.json")
output.write_text(json.dumps(manifest, indent=2) + "\n")
print(output)
