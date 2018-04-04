#!/bin/bash

DOCKER_BIN=`which docker`

function usage {
    echo "Usage: $0 <path-to-acrn>"
    echo "  Where <path-to-acrn> is the path to where you"
    echo "  have cloned the acrn repositories"
    exit 1
}

if [ $# -eq 0 ];
then
	usage
fi

build_container () {
	var=$(sudo $DOCKER_BIN images --format '{{.Tag}}' | grep -c $distro)
	echo $var
if [ $(sudo $DOCKER_BIN images --format '{{.Repository}}' | grep -c $distro) ==  '0' ];
then
	echo There is no build container for $distro yet.
	echo Creating a build container for $distro... please be patient!
	$DOCKER_BIN build -t $distro -f Dockerfile.$distro .
else
	echo We already have a build container for $distro, attempting to use it...
fi
}

for distro in `ls Dockerfile.*`; do
	# Extract the name of the Linux distro. It assumes the Dockerfile name is built as "Dockerfile.<distro>"
	distro=${distro:11}
	build_container
	echo "Testing Linux distribion: $distro"
	$DOCKER_BIN run -v $1:/root/acrn:z $distro make
done

