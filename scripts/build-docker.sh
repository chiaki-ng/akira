#!/bin/bash
set -eo pipefail

AKIRA_PLATFORM="${AKIRA_PLATFORM:-nx}"
case "${AKIRA_PLATFORM}" in
    nx) source /opt/devkitpro/switchvars.sh ;;
    ounce) source /opt/devkitpro/switch2vars.sh ;;
    *) echo "AKIRA_PLATFORM must be nx or ounce, not ${AKIRA_PLATFORM}"; exit 1 ;;
esac

AKIRA_SHELL="${AKIRA_SHELL:-borealis}"
AKIRA_ARISA_GPU="${AKIRA_ARISA_GPU:-drako3d}"
case "${AKIRA_ARISA_GPU}" in
    drako3d|vulkan) ;;
    *) echo "AKIRA_ARISA_GPU must be drako3d or vulkan, not ${AKIRA_ARISA_GPU}"; exit 1 ;;
esac
case "${AKIRA_SHELL}" in
    borealis) BUILD_NAME="${AKIRA_PLATFORM}"; NRO_TARGET=akira ;;
    arisa) BUILD_NAME="arisa-${AKIRA_PLATFORM}"; NRO_TARGET=akira-arisa ;;
    *) echo "AKIRA_SHELL must be borealis or arisa, not ${AKIRA_SHELL}"; exit 1 ;;
esac

if [ "${AKIRA_SHELL}" = arisa ] && [ "${AKIRA_PLATFORM}" = ounce ] && [ "${AKIRA_ARISA_GPU}" = vulkan ]; then
    BUILD_NAME="arisa-ounce-vulkan"
fi

SOURCE_DIR="${AKIRA_SOURCE:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
BUILD_DIR="${SOURCE_DIR}/build/${BUILD_NAME}"
DOCKER_SCRIPTS="${SOURCE_DIR}/scripts/docker"
ENVIRONMENT_FILE="${BUILD_DIR}/.akira-environment"

git config --global --add safe.directory '*'

if [ "${AKIRA_PLATFORM}" = "nx" ]; then
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
fi

LIBRARY_SUFFIX=""
if [ "${AKIRA_PLATFORM}" = "ounce" ]; then
    LIBRARY_SUFFIX="-ounce"
fi
CORE_DIR="${WITH_CORE:-${SOURCE_DIR}/library/libchiaki-core}"
BOREALIS_DIR="${WITH_BOREALIS:-${SOURCE_DIR}/library/borealis${LIBRARY_SUFFIX}}"
WIREGUARD_DIR="${WITH_WIREGUARD:-${SOURCE_DIR}/library/switch-wireguard${LIBRARY_SUFFIX}}"
LWIP_DIR="${WITH_LWIP:-${SOURCE_DIR}/library/lwip}"

for override in CORE BOREALIS WIREGUARD LWIP UAM NX_DEPS CURL FFMPEG NGHTTP2 SIMDUTF; do
    variable="WITH_${override}"
    if [ -n "${!variable}" ]; then
        echo "=== Override ${override}: ${!variable} ==="
    fi
done

ENVIRONMENT="platform=${AKIRA_PLATFORM} shell=${AKIRA_SHELL} arisa=${AKIRA_ARISA_PREFIX} arisa_gpu=${AKIRA_ARISA_GPU} image=${AKIRA_IMAGE} source=${SOURCE_DIR} borealis=${BOREALIS_DIR} nx_deps=${WITH_NX_DEPS} curl=${WITH_CURL} ffmpeg=${WITH_FFMPEG} nghttp2=${WITH_NGHTTP2} simdutf=${WITH_SIMDUTF} uam=${WITH_UAM}"
if [ "$(cat "${ENVIRONMENT_FILE}" 2>/dev/null)" != "${ENVIRONMENT}" ]; then
    echo "=== Build environment changed, starting a clean build ==="
    rm -rf "${BUILD_DIR}"
fi

AKIRA_PLATFORM="${AKIRA_PLATFORM}" AKIRA_CORE_DIR="${CORE_DIR}" bash "${SOURCE_DIR}/scripts/write-build-info.sh" "${SOURCE_DIR}"

MUTE_CHIAKI_LOGS=OFF
if [ "${MUTE_CHIAKI}" = "true" ]; then
    MUTE_CHIAKI_LOGS=ON
fi

AKIRA_CLOUD=OFF
if [ "${CLOUD:-false}" = "true" ]; then
    AKIRA_CLOUD=ON
fi

AKIRA_PS5_CLOUD_LAUNCH=OFF
if [ "${PS5_CLOUD_LAUNCH:-false}" = "true" ]; then
    AKIRA_PS5_CLOUD_LAUNCH=ON
fi

AKIRA_PS4_CLOUD_LAUNCH=OFF
if [ "${PS4_CLOUD_LAUNCH:-false}" = "true" ]; then
    AKIRA_PS4_CLOUD_LAUNCH=ON
fi

cmake -G Ninja -S "${SOURCE_DIR}" -B "${BUILD_DIR}" \
    -DPLATFORM_SWITCH=ON \
    -DAKIRA_PLATFORM="${AKIRA_PLATFORM}" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBUILD_CHANNEL="${BUILD_CHANNEL:-dev}" \
    -DVERSION_PRERELEASE="${VERSION_PRERELEASE}" \
    -DMUTE_CHIAKI_LOGS="${MUTE_CHIAKI_LOGS}" \
    -DAKIRA_CLOUD="${AKIRA_CLOUD}" \
    -DAKIRA_PS5_CLOUD_LAUNCH="${AKIRA_PS5_CLOUD_LAUNCH}" \
    -DAKIRA_PS4_CLOUD_LAUNCH="${AKIRA_PS4_CLOUD_LAUNCH}" \
    -DAKIRA_CORE_DIR="${CORE_DIR}" \
    -DBOREALIS_DIR="${BOREALIS_DIR}" \
    -DWIREGUARD_DIR="${WIREGUARD_DIR}" \
    -DLWIP_DIR="${LWIP_DIR}" \
    -DAKIRA_SHELL="${AKIRA_SHELL}" \
    -DAKIRA_ARISA_PREFIX="${AKIRA_ARISA_PREFIX}" \
    -DAKIRA_ARISA_GPU="${AKIRA_ARISA_GPU}"
echo "${ENVIRONMENT}" > "${ENVIRONMENT_FILE}"

cmake --build "${BUILD_DIR}" --target "${NRO_TARGET}.nro"

ls -la "${BUILD_DIR}"/*.nro
