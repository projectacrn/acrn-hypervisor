FROM clearlinux:base

MAINTAINER shrmrf "https://github.com/shrmrf"

# Install packages for building acrn
# RUN swupd update
RUN swupd bundle-add  os-core-dev

RUN git config --global http.sslVerify false

COPY  . /root/acrn-hypervisor
RUN cd /root/acrn-hypervisor; make clean && make PLATFORM=uefi
