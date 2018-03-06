#!/bin/bash

# pull fresh copies of the ACRN source and copy public API headers
# over to the documentation tree

cd ../acrn-hypervisor;git pull

mkdir -p ../acrn_documentation/_source/hypervisor/include/common
cp include/common/hypercall.h ../acrn_documentation/_source/hypervisor/include/common

mkdir -p ../acrn_documentation/_source/hypervisor/include/public
cp include/public/acrn_common.h ../acrn_documentation/_source/hypervisor/include/public
cp include/public/acrn_hv_defs.h ../acrn_documentation/_source/hypervisor/include/public

cd ../acrn-devicemodel;git pull

mkdir -p ../acrn_documentation/_source/devicemodel/include
cp include/virtio.h ../acrn_documentation/_source/devicemodel/include
