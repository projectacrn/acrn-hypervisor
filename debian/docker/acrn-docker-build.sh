#!/bin/bash

# Helper script to build ACRN with docker
# This also includes building packages required for ACRN build or runtime

VENDOR=${VENDOR:-debian}
DISTRO=${DISTRO:-stable}

TOPDIR=$(git rev-parse --show-toplevel)
DOCKER=$(which docker)

if [ -z "${TOPDIR}" ]; then
    echo "Run $0 from inside git repository!"
    exit 1
fi

if [ -z "${DOCKER}" ]; then
    echo "Cannot find docker binary, please install!"
    exit 1
fi

pushd ${TOPDIR} >/dev/null

if [ ! -f debian/docker/Dockerfile ]; then
    echo "No Dockerfile available!"
    exit 1
fi

set -e
# create docker image for Debian package build
${DOCKER} build \
    -f debian/docker/Dockerfile \
    --build-arg DISTRO=${DISTRO} \
    --build-arg VENDOR=${VENDOR} \
    -t acrn-pkg-builder:${DISTRO} debian/docker

# build ACRN packages
${DOCKER} run \
    --rm \
    -e UID=$(id -u) \
    -e GID=$(id -g) \
    -v $(pwd):/source --entrypoint /usr/local/bin/debian-pkg-build.sh acrn-pkg-builder:${DISTRO} -F --no-sign --git-export-dir=build/${DISTRO} "$@"

# create local apt repository
${DOCKER} run \
    --rm \
    -e UID=$(id -u) \
    -e GID=$(id -g) \
    -v $(pwd):/source --entrypoint create-apt-repo.sh acrn-pkg-builder:${DISTRO} build/${DISTRO}

popd >/dev/null
