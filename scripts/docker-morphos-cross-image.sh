#!/bin/sh
#
# Copyright (c) 2026 Michele Dipace <michele.dipace@kaffeine.net>
# SPDX-License-Identifier: MIT
#
# Build the Docker image containing pkgsrc cross/ppc-morphos-gcc.

set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
IMAGE=${IMAGE:-telegram-amiga-morphos-cross:pkgsrc-2026Q1}
PKGSRC_BRANCH=${PKGSRC_BRANCH:-pkgsrc-2026Q1}
MAKE_JOBS=${MAKE_JOBS:-4}
ACCEPT_MORPHOS_SDK_LICENSE=${ACCEPT_MORPHOS_SDK_LICENSE:-0}
ACCEPT_LHA_LICENSE=${ACCEPT_LHA_LICENSE:-0}

if ! command -v docker >/dev/null 2>&1; then
    echo "docker not found. Install Docker or start Colima/Docker Desktop first." >&2
    exit 1
fi

if [ "$ACCEPT_MORPHOS_SDK_LICENSE" != "1" ]; then
    echo "MorphOS SDK license not accepted." >&2
    echo "Review the MorphOS SDK license, then rerun with:" >&2
    echo "  ACCEPT_MORPHOS_SDK_LICENSE=1 $0" >&2
    exit 1
fi

if [ "$ACCEPT_LHA_LICENSE" != "1" ]; then
    echo "lha license not accepted." >&2
    echo "Review the lha license, then rerun with:" >&2
    echo "  ACCEPT_LHA_LICENSE=1 $0" >&2
    exit 1
fi

docker build \
    --build-arg "PKGSRC_BRANCH=$PKGSRC_BRANCH" \
    --build-arg "MAKE_JOBS=$MAKE_JOBS" \
    --build-arg "ACCEPT_MORPHOS_SDK_LICENSE=$ACCEPT_MORPHOS_SDK_LICENSE" \
    --build-arg "ACCEPT_LHA_LICENSE=$ACCEPT_LHA_LICENSE" \
    -t "$IMAGE" \
    -f "$ROOT_DIR/docker/morphos-cross/Dockerfile" \
    "$ROOT_DIR"
