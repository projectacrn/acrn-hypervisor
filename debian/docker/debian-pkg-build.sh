#!/bin/bash -e

if [ ! -f debian/control ]; then
    echo "Cannot find debian/control" >&2
    exit 1
fi

if [[ -n ${UID} && -n ${GID} ]]; then
    addgroup --gid ${GID} docker-build
    adduser --uid=${UID} --gid=${GID} --disabled-password --gecos '' docker-build
else
    echo "UID/GID not set. Use docker run -e UID=$(id -u) -e GID=$(id -g)" >&2
    exit 1
fi

# install build dependencies using tmpdir to not interfer with parallel builds
topdir=$(pwd)
tmpdir=$(mktemp -d)
pushd ${tmpdir} >/dev/null
mk-build-deps --tool='apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends --yes' --install ${topdir}/debian/control
popd >/dev/null
rm -rf ${tmpdir}

# start build
export HOME=$(echo ~docker-build)
sudo -E -u docker-build gbp buildpackage "$@"
