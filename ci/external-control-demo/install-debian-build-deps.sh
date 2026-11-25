#!/bin/sh
set -eu

export DEBIAN_FRONTEND=noninteractive
apt-get update
set --
if apt-cache show libunarr-dev >/dev/null 2>&1; then
  set -- libunarr-dev
fi
apt-get install -y --no-install-recommends \
  autoconf \
  autoconf-archive \
  automake \
  binutils \
  build-essential \
  ca-certificates \
  cmake \
  dbus \
  dbus-x11 \
  file \
  gettext \
  git \
  googletest \
  libgtest-dev \
  libarchive-dev \
  libbz2-dev \
  libcxx-serial-dev \
  libcurl4-openssl-dev \
  libdrm-dev \
  libelf-dev \
  libexif-dev \
  libgdk-pixbuf-2.0-dev \
  libglew-dev \
  libgtk-3-dev \
  libjs-highlight.js \
  libjs-mathjax \
  libjsoncpp-dev \
  libltdl-dev \
  liblz4-dev \
  liblzma-dev \
  locales \
  lsb-release \
  libpango1.0-dev \
  libshp-dev \
  libsqlite3-dev \
  libssl-dev \
  libtinyxml-dev \
  libtool \
  libudev-dev \
  libusb-1.0-0-dev \
  libwxgtk3.2-dev \
  libwxgtk-webview3.2-dev \
  libwxsvg-dev \
  ninja-build \
  nlohmann-json3-dev \
  pkg-config \
  portaudio19-dev \
  python-is-python3 \
  python3-dbus \
  python3-gi \
  rapidjson-dev \
  uuid-dev \
  zlib1g-dev \
  xauth \
  xvfb "$@"

localedef -i en_US -f UTF-8 en_US.UTF-8
localedef -i sv_SE -f UTF-8 sv_SE.UTF-8

rm -rf /var/lib/apt/lists/*
