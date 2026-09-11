#!/usr/bin/env bash
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
. /etc/os-release
case "$ID:$VERSION_ID" in
  debian:13|ubuntu:22.04|ubuntu:24.04) ;;
  *) echo "Unsupported qualification distribution: $ID $VERSION_ID" >&2; exit 1 ;;
esac
if [[ "$ID:$VERSION_ID" == ubuntu:22.04 ]]; then
  # Jammy's distribution wxWidgets is too old; use OpenCPN's signed PPA.
  apt-get install -y --no-install-recommends software-properties-common gnupg
  add-apt-repository -y ppa:opencpn/opencpn
fi
ci/external-control-demo/install-debian-build-deps.sh
apt-get update
# Development packages select each distro's matching runtime SONAME packages.
# Do not hardcode Bookworm's libnetcdf19/libhdf5-103-1/libzip4 on newer distros.
apt-get install -y --no-install-recommends \
  curl iproute2 jq libaec-dev libblosc-dev libeccodes-dev libhdf5-dev \
  libgdk-pixbuf2.0-bin libgtest-dev libnetcdf-dev libproj-dev libqhull-dev \
  libsodium-dev librsvg2-common libzip-dev libzstd-dev libvulkan-dev \
  python3-build python3-pip python3-setuptools python3-tk python3-venv \
  util-linux xauth
pixbuf_query="/usr/lib/$(dpkg-architecture -qDEB_HOST_MULTIARCH)/gdk-pixbuf-2.0/gdk-pixbuf-query-loaders"
"$pixbuf_query" --update-cache
grep -q libpixbufloader-svg.so "$(dirname "$pixbuf_query")/2.10.0/loaders.cache"
python3 -m venv "$GITHUB_WORKSPACE/.ci-python"
"$GITHUB_WORKSPACE/.ci-python/bin/pip" install \
  'packaging==25.0' 'setuptools==80.9.0' 'wheel==0.45.1' 'build==1.2.2.post1'
echo "$GITHUB_WORKSPACE/.ci-python/bin" >> "$GITHUB_PATH"
