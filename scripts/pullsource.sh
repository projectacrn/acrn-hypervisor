#!/bin/bash

q="--quiet"

# pull fresh copies of the ACRN source for use by doxygen

if [ ! -d "../acrn-hypervisor" ]; then
  echo Repo for acrn-hypervisor is missing.
  exit -1
fi
if [ ! -d "../acrn-devicemodel" ]; then
  echo Repo for acrn-devicemodel is missing.
  exit -1
fi

# Assumes origin (personal) and upstream (project) remote repos are
# setup

cd ../acrn-hypervisor
git checkout $q master;
git fetch $q upstream
git merge $q upstream/master
git push $q origin master


cd ../acrn-devicemodel
git checkout $q master;
git fetch $q upstream
git merge $q upstream/master
git push $q origin master
