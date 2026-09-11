#!/usr/bin/env bash
set -euo pipefail
# Build probe only: not an installer and never a release upload. All plugins,
# helpers, signing and isolated profile integration must qualify separately.
brew install cmake ninja gettext create-dmg
mkdir -p inputs build-platform evidence
curl --fail --location --retry 3 \
  https://dl.cloudsmith.io/public/nohal/opencpn-dependencies/raw/files/macos_deps_universal-opencpn.tar.xz \
  --output inputs/macos-deps.tar.xz
shasum -a 256 inputs/macos-deps.tar.xz | tee evidence/dependencies.sha256
echo '1a9422ee632effb0a47eb5fbaaf2effaf0c26b321a8155f824453fbafb058dc6  inputs/macos-deps.tar.xz' | shasum -a 256 --check
# Keep upstream dependencies separate from Homebrew's symlinks and libraries.
deps_dir="$PWD/inputs/macos-deps"
mkdir -p "$deps_dir"
tar -C "$deps_dir" -xJf inputs/macos-deps.tar.xz
export PATH="$(brew --prefix gettext)/bin:$PATH"
export DYLD_LIBRARY_PATH="$deps_dir/lib${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
cmake -S . -B build-platform -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_CXX_FLAGS=-Wno-error=inconsistent-missing-override \
  -DCMAKE_INSTALL_PREFIX="$PWD/stage-platform" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DCMAKE_OSX_ARCHITECTURES="$PREVIEW_ARCH" \
  -DOCPN_CI_BUILD=ON -DOCPN_RELEASE=0 \
  -DOCPN_USE_VULKAN_PRESENTER=OFF -DOCPN_USE_GL=ON \
  -DOCPN_BUILD_TEST=ON -DOCPN_USE_DEPS_BUNDLE=ON \
  -DOCPN_USE_SYSTEM_LIBARCHIVE=OFF \
  -DOCPN_DEPS_BUNDLE_PATH="$deps_dir" \
  -DCMAKE_PREFIX_PATH="$deps_dir" \
  -DwxWidgets_CONFIG_EXECUTABLE="$deps_dir/lib/wx/config/osx_cocoa-unicode-3.2" \
  -DwxWidgets_CONFIG_OPTIONS="--prefix=$deps_dir" \
  2>&1 | tee evidence/configure.log
cmake --build build-platform --parallel 3 2>&1 | tee evidence/build.log
build-platform/test/tests \
  --gtest_filter='ExternalApiTest.*:InProcessPlanningJobServiceTest.*:BoundedApplicationEventStreamTest.*:ChartSafetyDepth.*:ChartSafetyService.*' \
  --gtest_output=xml:evidence/core-tests.xml 2>&1 | tee evidence/tests.log
python3 ci/external-control-demo/verify-test-report.py evidence/core-tests.xml
git rev-parse HEAD > evidence/source-commit.txt
file build-platform/OpenCPN.app/Contents/MacOS/OpenCPN | tee evidence/architecture.txt
otool -L build-platform/OpenCPN.app/Contents/MacOS/OpenCPN > evidence/dependencies.txt
