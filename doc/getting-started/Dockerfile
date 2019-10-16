# Build container based on Clearlinux
FROM clearlinux:base

# Tools (bundles) to build the ACRN hypervisor, Device Model, tools and doc
# - bundles needed to build kernels: bc, lz4, diffutils and devpkg-elfutils, devpkg-ncurses
# - bundles needed to build the documentation: doxygen, graphviz, desktop-apps
RUN swupd update -b && \
	swupd bundle-add -b c-basic \
			python3-basic \
			which \
			git \
			devpkg-systemd \
			devpkg-telemetrics-client \
			devpkg-e2fsprogs \
			devpkg-libxml2 \
			devpkg-openssl \
			devpkg-util-linux \
			devpkg-libevent \
			devpkg-libusb \
			devpkg-libpciaccess \
			devpkg-gnu-efi \
			bc \
			lz4 \
			diffutils \
			devpkg-elfutils \
			doxygen \
			rsync \
			devpkg-ncurses \
			devpkg-graphviz \
			desktop-apps \
	&& rm -rf /var/lib/swupd/*

RUN pip3 install kconfiglib
# End of section installing build tools (bundles)

# Additional tools (Python) to build the documentation
RUN curl -O https://raw.githubusercontent.com/projectacrn/acrn-hypervisor/master/doc/scripts/requirements.txt && \
	pip3 install -r requirements.txt && \
	rm requirements.txt
# End of documentation generation tools

WORKDIR /workspace
