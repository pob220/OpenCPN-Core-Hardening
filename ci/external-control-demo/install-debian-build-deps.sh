#!/bin/sh
set -eu

export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
  autoconf \
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
  liblz4-dev \
  liblzma-dev \
  lsb-release \
  libpango1.0-dev \
  libshp-dev \
  libsqlite3-dev \
  libssl-dev \
  libtinyxml-dev \
  libtool \
  libudev-dev \
  libunarr-dev \
  libusb-1.0-0-dev \
  libwxgtk3.2-dev \
  libwxgtk-webview3.2-dev \
  libwxsvg-dev \
  ninja-build \
  nlohmann-json3-dev \
  portaudio19-dev \
  python-is-python3 \
  python3-dbus \
  python3-gi \
  rapidjson-dev \
  xvfb

rm -rf /var/lib/apt/lists/*
