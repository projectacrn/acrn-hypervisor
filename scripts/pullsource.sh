#!/bin/bash

# pull fresh copies of the ACRN source for use by doxygen

if [ ! -d "../acrn-hypervisor" ]; then
  echo Repo for acrn-hypervisor is missing.
  exit -1
fi
if [ ! -d "../acrn-devicemodel" ]; then
  echo Repo for acrn-devicemodel is missing.
  exit -1
fi

cd ../acrn-hypervisor;git pull
cd ../acrn-devicemodel;git pull
