# Build container based on Ubuntu 18.04
FROM ubuntu:18.04

# Install dependencies.
RUN apt-get update \
    && apt-get install -y gcc make vim git \
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
    && apt-get clean

RUN pip3 install kconfiglib

WORKDIR /root/acrn

CMD ["/bin/bash"]
