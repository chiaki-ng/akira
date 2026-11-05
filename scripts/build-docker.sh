#!/bin/bash
set -eo pipefail
source /opt/devkitpro/switchvars.sh

SOURCE_DIR="${AKIRA_SOURCE:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
BUILD_DIR="${SOURCE_DIR}/build"
DOCKER_SCRIPTS="${SOURCE_DIR}/scripts/docker"
ENVIRONMENT_FILE="${BUILD_DIR}/.akira-environment"

git config --global --add safe.directory '*'

if [ -n "${WITH_NX_DEPS}" ]; then
    bash "${DOCKER_SCRIPTS}/install-nx-deps.sh" pool "${WITH_NX_DEPS}"
fi

for library in curl ffmpeg nghttp2 simdutf; do
    variable="WITH_${library^^}"
    if [ -n "${!variable}" ]; then
        bash "${DOCKER_SCRIPTS}/install-nx-deps.sh" "${library}" "${!variable}"
    fi
done

if [ -n "${WITH_UAM}" ]; then
    bash "${DOCKER_SCRIPTS}/build-uam.sh" "${WITH_UAM}"
fi

CORE_DIR="${WITH_CORE:-${SOURCE_DIR}/library/libchiaki-core}"
BOREALIS_DIR="${WITH_BOREALIS:-${SOURCE_DIR}/library/borealis}"
WIREGUARD_DIR="${WITH_WIREGUARD:-${SOURCE_DIR}/library/switch-wireguard}"
LWIP_DIR="${WITH_LWIP:-${SOURCE_DIR}/library/lwip}"

for override in CORE BOREALIS WIREGUARD LWIP UAM NX_DEPS CURL FFMPEG NGHTTP2 SIMDUTF; do
    variable="WITH_${override}"
    if [ -n "${!variable}" ]; then
        echo "=== Override ${override}: ${!variable} ==="
    fi
done

ENVIRONMENT="image=${AKIRA_IMAGE} source=${SOURCE_DIR} nx_deps=${WITH_NX_DEPS} curl=${WITH_CURL} ffmpeg=${WITH_FFMPEG} nghttp2=${WITH_NGHTTP2} simdutf=${WITH_SIMDUTF} uam=${WITH_UAM}"
if [ "$(cat "${ENVIRONMENT_FILE}" 2>/dev/null)" != "${ENVIRONMENT}" ]; then
    echo "=== Build environment changed, starting a clean build ==="
    rm -rf "${BUILD_DIR}"
fi

AKIRA_CORE_DIR="${CORE_DIR}" bash "${SOURCE_DIR}/scripts/write-build-info.sh" "${SOURCE_DIR}"

MUTE_CHIAKI_LOGS=OFF
if [ "${MUTE_CHIAKI}" = "true" ]; then
    MUTE_CHIAKI_LOGS=ON
fi

AKIRA_PS5_CLOUD_LAUNCH=OFF
if [ "${PS5_CLOUD_LAUNCH:-false}" = "true" ]; then
    AKIRA_PS5_CLOUD_LAUNCH=ON
fi

cmake -G Ninja -S "${SOURCE_DIR}" -B "${BUILD_DIR}" \
    -DPLATFORM_SWITCH=ON \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBUILD_CHANNEL="${BUILD_CHANNEL:-dev}" \
    -DVERSION_PRERELEASE="${VERSION_PRERELEASE}" \
    -DMUTE_CHIAKI_LOGS="${MUTE_CHIAKI_LOGS}" \
    -DAKIRA_PS5_CLOUD_LAUNCH="${AKIRA_PS5_CLOUD_LAUNCH}" \
    -DAKIRA_CORE_DIR="${CORE_DIR}" \
    -DBOREALIS_DIR="${BOREALIS_DIR}" \
    -DWIREGUARD_DIR="${WIREGUARD_DIR}" \
    -DLWIP_DIR="${LWIP_DIR}"
echo "${ENVIRONMENT}" > "${ENVIRONMENT_FILE}"

cmake --build "${BUILD_DIR}" --target akira.nro

ls -la "${BUILD_DIR}"/*.nro
