#!/usr/bin/env bash
set -euo pipefail
set -x

cd "${HOME}/project"
test -n "${OCPN_TARGET:-}"
test -n "${DOCKER_IMAGE:-}"

mkdir -p build artifacts
docker build \
  --build-arg "BASE_IMAGE=$DOCKER_IMAGE" \
  -f ci/external-control-demo/Dockerfile.bookworm \
  -t opencpn-external-control-demo-linux-build \
  ci/external-control-demo
docker run --rm \
  -e "OCPN_TARGET=$OCPN_TARGET" \
  -e "EXPECTED_ELF_MACHINE=${EXPECTED_ELF_MACHINE:-AArch64}" \
  -e "CMAKE_BUILD_PARALLEL_LEVEL=${CMAKE_BUILD_PARALLEL_LEVEL:-2}" \
  -v "$PWD:/src:ro" \
  -v "$PWD/build:/work" \
  opencpn-external-control-demo-linux-build \
  /src/ci/external-control-demo/build-linux.sh

sudo chmod -R a+rw build
cp -a build/artifacts/. artifacts/

