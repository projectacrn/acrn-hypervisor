#!/bin/bash

# pull fresh copies of the ACRN source and copy public API headers
# over to the documentation tree

if [ ! -d "../acrn-hypervisor" ]; then
  echo Repo for acrn-hypervisor is missing.
  exit -1
fi
if [ ! -d "../acrn-devicemodel" ]; then
  echo Repo for acrn-devicemodel is missing.
  exit -1
fi

cd ../acrn-hypervisor;git pull

mkdir -p ../acrn-documentation/_source/hypervisor/include/common
cp include/common/hypercall.h ../acrn-documentation/_source/hypervisor/include/common

mkdir -p ../acrn-documentation/_source/hypervisor/include/public
cp include/public/acrn_common.h ../acrn-documentation/_source/hypervisor/include/public
cp include/public/acrn_hv_defs.h ../acrn-documentation/_source/hypervisor/include/public

cd ../acrn-devicemodel;git pull

mkdir -p ../acrn-documentation/_source/devicemodel/include
cp include/virtio.h ../acrn-documentation/_source/devicemodel/include
