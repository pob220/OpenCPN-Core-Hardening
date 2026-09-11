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
# Upstream ABI-compatible dependency layout; this is an ephemeral CI runner.
sudo tar -C /usr/local -xJf inputs/macos-deps.tar.xz
export PATH="$(brew --prefix gettext)/bin:$PATH"
cmake -S . -B build-platform -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX="$PWD/stage-platform" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DCMAKE_OSX_ARCHITECTURES="$PREVIEW_ARCH" \
  -DOCPN_CI_BUILD=ON -DOCPN_RELEASE=0 \
  -DOCPN_USE_VULKAN_PRESENTER=OFF -DOCPN_USE_GL=ON \
  -DOCPN_BUILD_TEST=ON -DOCPN_USE_DEPS_BUNDLE=ON \
  -DOCPN_USE_SYSTEM_LIBARCHIVE=OFF \
  -DOCPN_DEPS_BUNDLE_PATH=/usr/local \
  -DwxWidgets_CONFIG_EXECUTABLE=/usr/local/lib/wx/config/osx_cocoa-unicode-3.2 \
  -DwxWidgets_CONFIG_OPTIONS=--prefix=/usr/local \
  2>&1 | tee evidence/configure.log
cmake --build build-platform --parallel 3 2>&1 | tee evidence/build.log
build-platform/test/tests \
  --gtest_filter='ExternalApiTest.*:InProcessPlanningJobServiceTest.*:BoundedApplicationEventStreamTest.*:ChartSafetyDepth.*:ChartSafetyService.*' \
  --gtest_output=xml:evidence/core-tests.xml 2>&1 | tee evidence/tests.log
git rev-parse HEAD > evidence/source-commit.txt
file build-platform/OpenCPN.app/Contents/MacOS/OpenCPN | tee evidence/architecture.txt
otool -L build-platform/OpenCPN.app/Contents/MacOS/OpenCPN > evidence/dependencies.txt
