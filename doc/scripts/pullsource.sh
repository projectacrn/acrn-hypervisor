#!/bin/bash

q="--quiet"

# get the latest acrn-kernel sources for the kernel-doc API processing

if [ ! -d "../../acrn-kernel" ]; then
  echo Repo for acrn-kernel is missing.
  exit -1
fi

# Assumes origin is the upstream repo

cd ../../acrn-kernel
git checkout $q master
git fetch $q origin
git reset $q --hard origin/master
