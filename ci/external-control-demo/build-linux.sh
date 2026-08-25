#!/usr/bin/env bash
set -euo pipefail

source_dir=${1:-/src}
work_dir=${2:-/work}
target=${OCPN_TARGET:-bookworm-arm64}
expected_machine=${EXPECTED_ELF_MACHINE:-AArch64}
build_dir="$work_dir/build"
stage_dir="$work_dir/stage"
artifact_dir="$work_dir/artifacts/$target"
package_dir="$artifact_dir/package"
log_dir="$artifact_dir/logs"
test_dir="$artifact_dir/tests"

rm -rf "$build_dir" "$stage_dir" "$artifact_dir"
mkdir -p "$build_dir" "$stage_dir" "$package_dir" "$log_dir" "$test_dir"

# Container jobs check out the source as the runner user and build it as root.
# Mark only this exact checkout safe so version/provenance generation remains
# deterministic without weakening Git's ownership checks globally.
git config --global --add safe.directory "$source_dir"

cmake -S "$source_dir" -B "$build_dir" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DOCPN_CI_BUILD=ON \
  -DOCPN_DISTRO_BUILD=ON \
  -DOCPN_USE_BUNDLED_LIBS=OFF \
  -DOCPN_BUILD_SAMPLE=ON \
  2>&1 | tee "$log_dir/configure.log"

cmake --build "$build_dir" \
  --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}" \
  2>&1 | tee "$log_dir/build.log"

test_binary="$build_dir/test/tests"
test -x "$test_binary"
dbus-run-session "$test_binary" \
  --gtest_output="xml:$test_dir/external-control.xml" \
  --gtest_filter='ExternalApiTest.*:InProcessPlanningJobServiceTest.*:BoundedApplicationEventStreamTest.*:ChartSafetyDepth.*:ChartSafetyService.*' \
  2>&1 | tee "$log_dir/external-control-tests.log"

DESTDIR="$stage_dir" cmake --install "$build_dir" \
  2>&1 | tee "$log_dir/install.log"
cmake --build "$build_dir" --target package \
  2>&1 | tee "$log_dir/package.log"

package_archive=$(find "$build_dir" -maxdepth 1 -type f -name '*.tar.gz' -print -quit)
test -n "$package_archive"
"$source_dir/ci/external-control-demo/verify-core-plugin-package.sh" \
  "$stage_dir" "$package_archive" "$expected_machine" \
  2>&1 | tee "$log_dir/core-plugin-package-audit.log"

"$source_dir/ci/external-control-demo/verify-linux-architecture.sh" \
  "$stage_dir" "$expected_machine" \
  2>&1 | tee "$log_dir/architecture-audit.log"

find "$build_dir" -maxdepth 1 -type f \
  \( -name '*.sh' -o -name '*.tar.gz' -o -name '*.deb' \) \
  -exec cp -f '{}' "$package_dir/" \;

git -C "$source_dir" rev-parse HEAD > "$artifact_dir/source-commit.txt"
{
  printf 'target=%s\n' "$target"
  printf 'machine=%s\n' "$expected_machine"
  printf 'kernel_arch=%s\n' "$(uname -m)"
  printf 'distribution=%s\n' "$(. /etc/os-release && printf '%s %s' "$ID" "$VERSION_ID")"
} > "$artifact_dir/platform.txt"
sha256sum "$package_dir"/* > "$package_dir/SHA256SUMS"
