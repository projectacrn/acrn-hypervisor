# Build container based on Fedora 26
FROM fedora:26

RUN dnf -y update && dnf clean all
RUN dnf -y install gcc \
                   git \
                   make \
                   vim \
                   findutils \
                   libuuid-devel \
                   openssl-devel \
                   libpciaccess-devel \
                   gnu-efi-devel \
                   systemd-devel \
                   libxml2-devel \
                   libevent-devel \
                   libusbx-devel \
                   python3 \
                   python3-pip

RUN pip3 install kconfiglib

WORKDIR /root/acrn

CMD ["/bin/bash"]
