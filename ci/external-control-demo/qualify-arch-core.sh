#!/usr/bin/env bash
set -euo pipefail
# Ephemeral Arch CI container only; this is not an installer for a user's host.
mkdir -p evidence build-platform
pacman -Syu --noconfirm --needed \
  base-devel cmake ninja git gettext curl gtk3 wxwidgets-gtk3 lsb-release python \
  glew sqlite libarchive rapidjson nlohmann-json portaudio libsndfile libusb \
  libexif wxsvg bzip2 xz zlib dbus gtest mesa webkit2gtk-4.1 \
  vulkan-headers vulkan-icd-loader
pacman -Q > evidence/distribution-packages.txt
cmake -S . -B build-platform -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DOCPN_CI_BUILD=ON -DOCPN_RELEASE=0 -DOCPN_BUILD_TEST=ON \
  -DOCPN_USE_BUNDLED_LIBS=ON -DOCPN_USE_GL=ON \
  -DOCPN_USE_VULKAN_PRESENTER=ON \
  2>&1 | tee evidence/configure.log
cmake --build build-platform --parallel 3 2>&1 | tee evidence/build.log
dbus-run-session build-platform/test/tests \
  --gtest_filter='ExternalApiTest.*:InProcessPlanningJobServiceTest.*:BoundedApplicationEventStreamTest.*:ChartSafetyDepth.*:ChartSafetyService.*:RendererConfig*.*' \
  --gtest_output=xml:evidence/core-tests.xml 2>&1 | tee evidence/tests.log
python3 ci/external-control-demo/verify-test-report.py evidence/core-tests.xml
# actions/checkout and the container build user can have different ownership.
# Trust only this known checkout for this read; do not disable Git's check globally.
git -c safe.directory="$PWD" rev-parse HEAD > evidence/source-commit.txt
file build-platform/opencpn > evidence/architecture.txt
ldd build-platform/opencpn > evidence/dependencies.txt
! grep -q 'not found' evidence/dependencies.txt
