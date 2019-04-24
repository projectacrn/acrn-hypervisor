# Build container based on Ubuntu 16.04
FROM ubuntu:16.04

# Install dependencies.
RUN apt-get update \
    && apt-get upgrade -y  \
    && apt-get install -y gcc \
                          git \
                          make \
                          gnu-efi \
                          libssl-dev \
                          libpciaccess-dev \
                          uuid-dev \
                          libsystemd-dev \
                          libevent-dev \
                          libxml2-dev \
                          libusb-1.0-0-dev \
                          python3 \
                          python3-pip \
                          libblkid-dev \
                          e2fslibs-dev \
                          pkg-config \
    && apt-get clean

# Install gcc 7.3.*
RUN apt-get update \
    && apt-get install -y software-properties-common \
    && add-apt-repository ppa:ubuntu-toolchain-r/test \
    && apt-get update \
    && apt install g++-7 -y \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-7 60 \
                     --slave /usr/bin/g++ g++ /usr/bin/g++-7 \
    && apt-get clean

# Update binutils to 2.27 (no PPA found unfortunately)
RUN apt-get update \
    && apt-get install -y wget \
    && wget https://mirrors.ocf.berkeley.edu/gnu/binutils/binutils-2.27.tar.gz \
    && tar xzvf binutils-2.27.tar.gz \
    && cd binutils-2.27 \
    && ./configure \
    && make \
    && make install \
    && cd .. \
    && rm -fr binutils-2.27 \
    && apt-get clean

# Install header files for GPIO
RUN apt-get update \
    && apt-get install -y openwince-include \
    && cp /usr/include/openwince/arm/sa11x0/gpio.h /usr/include/linux/ \
    && cp /usr/include/openwince/common.h /usr/include/ \
    && apt-get clean

RUN pip3 install kconfiglib

WORKDIR /root/acrn

CMD ["/bin/bash"]
