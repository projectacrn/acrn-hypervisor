# Build container based on CentOS 7
FROM centos:centos7

RUN yum -y install https://dl.fedoraproject.org/pub/epel/epel-release-latest-7.noarch.rpm
RUN yum -y update; yum clean all
RUN yum -y install gcc \
                   git \
                   make \
                   vim \
                   libuuid-devel \
                   openssl-devel \
                   libpciaccess-devel \
                   gnu-efi-devel \
                   systemd-devel \
                   libxml2-devel \
                   libevent-devel \
                   libusbx-devel \
                   python34 \
                   python34-pip

RUN pip3 install kconfiglib

WORKDIR /root/acrn

CMD ["/bin/bash"]
