# Build container based on Clearlinux
FROM clearlinux:base

RUN swupd bundle-add -b os-clr-on-clr python3-basic

RUN pip3 install kconfiglib

WORKDIR /root/acrn

CMD ["/bin/bash"]
