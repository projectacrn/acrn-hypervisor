#!/bin/bash -e

# helper to create local APT repository includeing all package available in
# the docker images's local apt repo

if [ $# -ne 1 ]; then
   echo "Call with: $(basename ${BASH_SOURCE[0]}) <path to local apt directory>" >&2
   exit 1
fi

if [ ! -d $1 ]; then
   echo "Please create target directory $1 before calling this script!" >&2
   exit 1
fi

if [[ -n ${UID} && -n ${GID} ]]; then
    addgroup --gid ${GID} docker-build
    adduser --uid=${UID} --gid=${GID} --disabled-password --gecos '' docker-build
else
    echo "UID/GID not set. Use docker run -e UID=$(id -u) -e GID=$(id -g)" >&2
    exit 1
fi

# copy all Debian packages in local APT repo and create local APT repository
export HOME=$(echo ~docker-build)
sudo -E -u docker-build /bin/bash -c "\
    cd $1 && cp /opt/apt/*.deb . && \
    apt-ftparchive packages . > Packages && \
    cp /opt/apt/.Release.header Release && \
    apt-ftparchive release . >> Release"

