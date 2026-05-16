# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT

FROM debian:bookworm

ARG PKGSRC_BRANCH=pkgsrc-2026Q1
ARG PKGSRC_PREFIX=/opt/pkg
ARG MAKE_JOBS=4
ARG ACCEPT_MORPHOS_SDK_LICENSE=0
ARG ACCEPT_LHA_LICENSE=0

ENV DEBIAN_FRONTEND=noninteractive
ENV PATH="${PKGSRC_PREFIX}/gg/bin:${PKGSRC_PREFIX}/bin:${PKGSRC_PREFIX}/sbin:${PATH}"

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        bison \
        build-essential \
        ca-certificates \
        curl \
        file \
        flex \
        gawk \
        git \
        gzip \
        m4 \
        make \
        patch \
        perl \
        rsync \
        sed \
        tar \
        xz-utils && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /usr

RUN curl -fsSL "https://cdn.NetBSD.org/pub/pkgsrc/${PKGSRC_BRANCH}/pkgsrc.tar.gz" | \
        tar -xz && \
    cd /usr/pkgsrc/bootstrap && \
    ./bootstrap --prefix "${PKGSRC_PREFIX}" --pkgdbdir "${PKGSRC_PREFIX}/pkgdb"

RUN if [ "${ACCEPT_MORPHOS_SDK_LICENSE}" != "1" ]; then \
        echo "MorphOS SDK license not accepted."; \
        echo "Rebuild with ACCEPT_MORPHOS_SDK_LICENSE=1 after reviewing the MorphOS SDK license."; \
        echo "pkgsrc package: /usr/pkgsrc/cross/ppc-morphos-sdk"; \
        exit 1; \
    fi && \
    if [ "${ACCEPT_LHA_LICENSE}" != "1" ]; then \
        echo "lha license not accepted."; \
        echo "Rebuild with ACCEPT_LHA_LICENSE=1 after reviewing the lha license."; \
        echo "pkgsrc package: /usr/pkgsrc/archivers/lha"; \
        exit 1; \
    fi && \
    echo "ACCEPTABLE_LICENSES+= morphos-sdk-license" >> "${PKGSRC_PREFIX}/etc/mk.conf" && \
    echo "ACCEPTABLE_LICENSES+= lha-license" >> "${PKGSRC_PREFIX}/etc/mk.conf" && \
    cd /usr/pkgsrc/cross/ppc-morphos-gcc && \
    bmake MAKE_JOBS="${MAKE_JOBS}" install clean clean-depends

WORKDIR /work

CMD ["sh", "-lc", "make -f Makefile.morphos-cross clean all"]
