# use debian stable as default distribution
ARG DISTRO=stable
ARG VENDOR=debian

###############################################################################
# Prepare apt config and eventually repackage
FROM ${VENDOR}:${DISTRO} AS tool-builder

ARG DISTRO
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get -y update && \
    apt-get install -y --no-install-recommends ca-certificates

###############################################################################
# For older distros: Add backports packages needed
#  * debhelper dwz libdebhelper-perl: build tool releate update
#  * lintian: to keep an acceptable recent version
#  * linux-libc-dev: acrn-dm needs >= 4.20 for udmabuf UAPI
RUN REPOBASE=$(cat /etc/apt/sources.list | grep -v -E '^#' | head -1 | awk '{print $3}') && \
    eval $(cat /etc/os-release) && \
    if [ -n "${VERSION_CODENAME}" ]; then \
        DISTRONAME=${VERSION_CODENAME}; \
    else \
        DISTRONAME=${REPOBASE}; \
    fi && \
    case ${DISTRONAME} in \
        buster) \
            echo "deb https://deb.debian.org/debian ${DISTRONAME}-backports main" > /etc/apt/sources.list.d/${DISTRONAME}-backports.list; \
            for p in debhelper dwz libdebhelper-perl lintian linux-libc-dev; do \
                (echo "Package: $p"; \
                 echo "Pin: release a=${DISTRONAME}-backports"; \
                 echo "Pin-Priority: 900"; \
                 echo "") >> /etc/apt/preferences.d/pin-${DISTRONAME}-backports; \
            done; \
            ;; \
        focal) \
            for p in debhelper dwz libdebhelper-perl lintian; do \
                (echo "Package: $p"; \
                 echo "Pin: release a=${DISTRONAME}-backports"; \
                 echo "Pin-Priority: 900"; \
                 echo "") >> /etc/apt/preferences.d/pin-${DISTRONAME}-backports; \
            done; \
            ;; \
    esac

###############################################################################
# Install packages needed for git buildpackage based build
RUN apt-get -y update && \
    apt-get install -y --no-install-recommends \
        build-essential git-buildpackage devscripts dpkg-dev equivs \
        lintian sudo apt-utils pristine-tar


###############################################################################
# Repackage packages

# prepare local apt repo for backported packages
RUN mkdir -p /opt/apt && cd /opt/apt && \
    echo "Origin: ACRN Local Build" > .Release.header && \
    echo "Label: acrn-local-build" >> .Release.header && \
    apt-ftparchive packages . > Packages && \
    apt-ftparchive sources . > Sources && \
    (cat .Release.header && apt-ftparchive release .) > Release && \
    echo "deb [trusted=yes] file:/opt/apt ./" > /etc/apt/sources.list.d/acrn-local.list && \
    echo "deb-src [trusted=yes] file:/opt/apt ./" >> /etc/apt/sources.list.d/acrn-local.list && \
    touch /etc/apt/preferences.d/pin-acrn

# setup git config for temporary use
RUN git config --global user.name "ACRN Debian Package Build" && \
    git config --global user.email "acrn-dev@lists.projectacrn.org"

# elementpath >=2.5.0
RUN NEEDEDVERSION="2.5.0"; \
    PKGVERSION=$(apt-cache policy python3-elementpath | grep "Candidate:" | awk '{ print $2}'); \
    if [ -z "${PKGVERSION}" -o "${NEEDEDVERSION}" != "$(echo ${NEEDEDVERSION}\\n${PKGVERSION} | sort -V | head -n1)" ]; then \
        srcpkg="elementpath" && \
        url="https://salsa.debian.org/debian/${srcpkg}.git" && \
        upstream_tag="upstream/${NEEDEDVERSION}" && \
        debian_tag="debian/${NEEDEDVERSION}-1" && \
        debian_branch="master" && \
        upstream_branch="upstream" && \
        mkdir -p /usr/src/${srcpkg} && cd /usr/src/${srcpkg} && \
        git init && git remote add origin ${url} && \
        git fetch origin --depth 1 refs/tags/${upstream_tag}:refs/tags/${upstream_tag} && \
        git fetch origin --depth 1 refs/tags/${debian_tag}:refs/tags/${debian_tag} && \
        if git show ${debian_tag}:debian | grep -qw gbp.conf; then \
            pristine_tar=$(git show ${debian_tag}:debian/gbp.conf | awk -F "=" '/pristine-tar/ {print $2}' | tr '[:upper:]' '[:lower:]' | xargs); \
            if [ "${pristine_tar}" = "true" ]; then \
                git fetch origin pristine-tar; git branch -t pristine-tar origin/pristine-tar; \
            fi; \
            debian_branch=$(git show ${debian_tag}:debian/gbp.conf | awk -F "=" '/debian-branch/ {print $2}' | xargs) && \
            if [ -z "${debian_branch}" ]; then \
                debian_branch="master"; \
            fi; \
            upstream_branch=$(git show ${debian_tag}:debian/gbp.conf | awk -F "=" '/upstream-branch/ {print $2}' | xargs) && \
            if [ -z "${upstream_branch}" ]; then \
                upstream_branch="upstream"; \
            fi; \
        fi && \
        git checkout -b ${upstream_branch} ${upstream_tag} && \
        git checkout -b ${debian_branch} ${debian_tag} && \
        mk-build-deps --tool='apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends --yes' --install debian/control --remove && \
        rm -f $(dpkg-parsechangelog -Ssource)-build-deps_$(dpkg-parsechangelog -Sversion)_*.* && \
        DEB_BUILD_OPTIONS="nocheck" gbp buildpackage -F -us -uc  && \
        for p in $(grep -E '^Package:' debian/control | awk '{print $2}'); do \
            echo "Package: $p" >> /etc/apt/preferences.d/pin-acrn; \
            echo "Pin: release l=acrn-local-build" >> /etc/apt/preferences.d/pin-acrn; \
            echo "Pin-Priority: 900" >> /etc/apt/preferences.d/pin-acrn; \
            echo "" >> /etc/apt/preferences.d/pin-acrn; \
        done && \
        cd /usr/src && \
        mv *.deb /opt/apt && \
        mv ${srcpkg}_*.dsc /opt/apt && \
        mv ${srcpkg}_*.tar.* /opt/apt && \
        cd /opt/apt && \
        apt-ftparchive packages . > Packages && \
        apt-ftparchive sources . > Sources && \
        (cat .Release.header && apt-ftparchive release .) > Release && \
        apt-get update -y; \
    fi

# xmlschema >=1.10.0
RUN NEEDEDVERSION="1.10.0"; \
    PKGVERSION=$(apt-cache policy python3-xmlschema | grep "Candidate:" | awk '{ print $2}'); \
    if [ -z "${PKGVERSION}" -o "${NEEDEDVERSION}" != "$(echo ${NEEDEDVERSION}\\n${PKGVERSION} | sort -V | head -n1)" ]; then \
        srcpkg="xmlschema" && \
        url="https://salsa.debian.org/python-team/packages/python-${srcpkg}.git" && \
        upstream_tag="upstream/${NEEDEDVERSION}" && \
        debian_tag="debian/${NEEDEDVERSION}-1" && \
        debian_branch="master" && \
        upstream_branch="upstream" && \
        mkdir -p /usr/src/${srcpkg} && cd /usr/src/${srcpkg} && \
        git init && git remote add origin ${url} && \
        git fetch origin --depth 1 refs/tags/${upstream_tag}:refs/tags/${upstream_tag} && \
        git fetch origin --depth 1 refs/tags/${debian_tag}:refs/tags/${debian_tag} && \
        if git show ${debian_tag}:debian | grep -qw gbp.conf; then \
            pristine_tar=$(git show ${debian_tag}:debian/gbp.conf | awk -F "=" '/pristine-tar/ {print $2}' | tr '[:upper:]' '[:lower:]' | xargs); \
            if [ "${pristine_tar}" = "true" ]; then \
                git fetch origin pristine-tar; git branch -t pristine-tar origin/pristine-tar; \
            fi; \
            debian_branch=$(git show ${debian_tag}:debian/gbp.conf | awk -F "=" '/debian-branch/ {print $2}' | xargs) && \
            if [ -z "${debian_branch}" ]; then \
                debian_branch="master"; \
            fi; \
            upstream_branch=$(git show ${debian_tag}:debian/gbp.conf | awk -F "=" '/upstream-branch/ {print $2}' | xargs) && \
            if [ -z "${upstream_branch}" ]; then \
                upstream_branch="upstream"; \
            fi; \
        fi && \
        git checkout -b ${upstream_branch} ${upstream_tag} && \
        git checkout -b ${debian_branch} ${debian_tag} && \
        mk-build-deps --tool='apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends --yes' --install debian/control --remove && \
        rm -f $(dpkg-parsechangelog -Ssource)-build-deps_$(dpkg-parsechangelog -Sversion)_*.* && \
        DEB_BUILD_OPTIONS="nocheck" gbp buildpackage -F -us -uc  && \
        for p in $(grep -E '^Package:' debian/control | awk '{print $2}'); do \
            echo "Package: $p" >> /etc/apt/preferences.d/pin-acrn; \
            echo "Pin: release l=acrn-local-build" >> /etc/apt/preferences.d/pin-acrn; \
            echo "Pin-Priority: 900" >> /etc/apt/preferences.d/pin-acrn; \
            echo "" >> /etc/apt/preferences.d/pin-acrn; \
        done && \
        cd /usr/src && \
        mv *.deb /opt/apt && \
        mv python-${srcpkg}_*.dsc /opt/apt && \
        mv python-${srcpkg}_*.tar.* /opt/apt && \
        cd /opt/apt && \
        apt-ftparchive packages . > Packages && \
        apt-ftparchive sources . > Sources && \
        (cat .Release.header && apt-ftparchive release .) > Release && \
        apt-get update -y; \
    fi

# acpica-unix >= 20200925
RUN NEEDEDVERSION="20200925"; \
    PKGVERSION=$(apt-cache policy acpica-tools | grep "Candidate:" | awk '{ print $2}'); \
    if [ -z "${PKGVERSION}" -o "${NEEDEDVERSION}" != "$(echo ${NEEDEDVERSION}\\n${PKGVERSION} | sort -V | head -n1)" ]; then \
        srcpkg="acpica-unix" && \
        url="https://github.com/ahs3/acpica-tools" && \
        upstream_tag="upstream/${NEEDEDVERSION}" && \
        debian_tag="debian/${NEEDEDVERSION}-1" && \
        debian_branch="master" && \
        upstream_branch="upstream" && \
        mkdir -p /usr/src/${srcpkg} && cd /usr/src/${srcpkg} && \
        git init && git remote add origin ${url} && \
        git fetch origin --depth 1 refs/tags/${upstream_tag}:refs/tags/${upstream_tag} && \
        git fetch origin --depth 1 refs/tags/${debian_tag}:refs/tags/${debian_tag} && \
        if git show ${debian_tag}:debian | grep -qw gbp.conf; then \
            pristine_tar=$(git show ${debian_tag}:debian/gbp.conf | awk -F "=" '/pristine-tar/ {print $2}' | tr '[:upper:]' '[:lower:]' | xargs); \
            if [ "${pristine_tar}" = "true" ]; then \
                git fetch origin pristine-tar; git branch -t pristine-tar origin/pristine-tar; \
            fi; \
            debian_branch=$(git show ${debian_tag}:debian/gbp.conf | awk -F "=" '/debian-branch/ {print $2}' | xargs) && \
            if [ -z "${debian_branch}" ]; then \
                debian_branch="master"; \
            fi; \
            upstream_branch=$(git show ${debian_tag}:debian/gbp.conf | awk -F "=" '/upstream-branch/ {print $2}' | xargs) && \
            if [ -z "${upstream_branch}" ]; then \
                upstream_branch="upstream"; \
            fi; \
        fi && \
        git checkout -b ${upstream_branch} ${upstream_tag} && \
        git checkout -b ${debian_branch} ${debian_tag} && \
        mk-build-deps --tool='apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends --yes' --install debian/control --remove && \
        rm -f $(dpkg-parsechangelog -Ssource)-build-deps_$(dpkg-parsechangelog -Sversion)_*.* && \
        DEB_BUILD_OPTIONS="nocheck" gbp buildpackage -F -us -uc  && \
        for p in $(grep -E '^Package:' debian/control | awk '{print $2}'); do \
            echo "Package: $p" >> /etc/apt/preferences.d/pin-acrn; \
            echo "Pin: release l=acrn-local-build" >> /etc/apt/preferences.d/pin-acrn; \
            echo "Pin-Priority: 900" >> /etc/apt/preferences.d/pin-acrn; \
            echo "" >> /etc/apt/preferences.d/pin-acrn; \
        done && \
        cd /usr/src && \
        mv *.deb /opt/apt && \
        mv ${srcpkg}_*.dsc /opt/apt && \
        mv ${srcpkg}_*.tar.* /opt/apt && \
        cd /opt/apt && \
        apt-ftparchive packages . > Packages && \
        apt-ftparchive sources . > Sources && \
        (cat .Release.header && apt-ftparchive release .) > Release && \
        apt-get update -y; \
    fi

###############################################################################
# the final image
FROM ${VENDOR}:${DISTRO}

ARG VENDOR
ARG DISTRO

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get -y update && \
    apt-get install -y --no-install-recommends ca-certificates

COPY --from=tool-builder /etc/apt/sources.list.d/* /etc/apt/sources.list.d/
COPY --from=tool-builder /etc/apt/preferences.d/* /etc/apt/preferences.d/
COPY --from=tool-builder /opt/apt /opt/apt

###############################################################################
# Install build script requirements
RUN apt-get -y update && apt-get install -y --no-install-recommends \
    devscripts \
    equivs \
    git-buildpackage \
    lintian \
    apt-utils \
    sudo

###############################################################################
# pre-install build dependencies
COPY debian-control-${VENDOR}-${DISTRO} /tmp/debian-control
RUN tmpdir=$(mktemp -d) && cd ${tmpdir} && \
    mk-build-deps --tool='apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends --yes' --install /tmp/debian-control && \
    cd / && rm -rf ${tmpdir}

###############################################################################
# cleanup apt cache
RUN apt-get clean && rm -rf /var/lib/apt/lists/*


###############################################################################
# Mount the topdir of the Debian git repository at /source
VOLUME /source/
WORKDIR /source/

###############################################################################
# Get default settings and helper scripts
ADD gbp.conf /etc/git-buildpackage/
ADD debian-pkg-build.sh /usr/local/bin/debian-pkg-build.sh
ADD create-apt-repo.sh /usr/local/bin/create-apt-repo.sh
ADD lintian.sh /usr/local/bin/lintian.sh

