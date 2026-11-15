#!/bin/bash
set -e

BUILD_DIR="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
LIBRARY_SUFFIX=""
if [ "${AKIRA_PLATFORM:-nx}" = "ounce" ]; then
    LIBRARY_SUFFIX="-ounce"
fi
CORE_SOURCE_DIR="${AKIRA_CORE_DIR:-${BUILD_DIR}/library/libchiaki-core${LIBRARY_SUFFIX}}"

# Generate build info file with all installed packages
echo "=== Generating build info ==="

CORE_COMMIT=$(git -C "${CORE_SOURCE_DIR}" rev-parse --short HEAD 2>/dev/null || echo "unknown")
CORE_VERSION=$(tr -d '[:space:]' < "${CORE_SOURCE_DIR}/VERSION.txt" 2>/dev/null || echo "unknown")
AKIRA_PLATFORM="${AKIRA_PLATFORM:-nx}"
if [ "${AKIRA_PLATFORM}" = "ounce" ]; then
    OUNCE_DEPS="${DEVKITPRO:-/opt/devkitpro}/portlibs/switch2/share/ounce-deps"
    OUNCE_SDK_VERSION=$(sed -n 's/^ounce-devkit //p' "${DEVKITPRO:-/opt/devkitpro}/ounce-devkit-version.txt" 2>/dev/null || echo "unknown")
else
    CURL_VERSION=$(jq -r '.version' ${NX_DEPS}/curl/BUILD-INFO 2>/dev/null || echo "unknown")
    FFMPEG_VERSION=$(jq -r '.version' ${NX_DEPS}/ffmpeg/BUILD-INFO 2>/dev/null || echo "unknown")
    NGHTTP2_VERSION=$(jq -r '.version' ${NX_DEPS}/nghttp2/BUILD-INFO 2>/dev/null || echo "unknown")
fi

# Extract Akira version from cmake/version.cmake (channel/prerelease/commit composed below)
AKIRA_VERSION_KEY=$(echo "${AKIRA_PLATFORM}" | tr '[:lower:]' '[:upper:]')
AKIRA_CORE=$(sed -n "s/^set(AKIRA_VERSION_${AKIRA_VERSION_KEY} \"\(.*\)\")$/\1/p" ${BUILD_DIR}/cmake/version.cmake)

AKIRA_CHANNEL="${BUILD_CHANNEL:-dev}"
AKIRA_PRERELEASE="${VERSION_PRERELEASE:-}"

# Get Akira git commit SHA
AKIRA_COMMIT=$(cd ${BUILD_DIR} && git rev-parse --short HEAD 2>/dev/null || echo "unknown")

AKIRA_VERSION="${AKIRA_CORE}"
if [ -n "$AKIRA_PRERELEASE" ]; then
    AKIRA_VERSION="${AKIRA_VERSION}-${AKIRA_PRERELEASE}"
fi
if [ "$AKIRA_COMMIT" != "unknown" ]; then
    AKIRA_VERSION="${AKIRA_VERSION}+${AKIRA_COMMIT}"
fi

echo "Akira ${AKIRA_VERSION} (${AKIRA_CHANNEL})" > ${BUILD_DIR}/resources/build_info.txt
echo "Commit: ${AKIRA_COMMIT}" >> ${BUILD_DIR}/resources/build_info.txt
echo "Build: $(date -u '+%Y-%m-%d %H:%M:%S UTC')" >> ${BUILD_DIR}/resources/build_info.txt
echo "" >> ${BUILD_DIR}/resources/build_info.txt
echo "=== Custom Libraries ===" >> ${BUILD_DIR}/resources/build_info.txt
echo "libchiaking ${CORE_VERSION} (${CORE_COMMIT})" >> ${BUILD_DIR}/resources/build_info.txt
if [ "${AKIRA_PLATFORM}" = "ounce" ]; then
    echo "ounce-devkit ${OUNCE_SDK_VERSION}" >> ${BUILD_DIR}/resources/build_info.txt
    for info in "${OUNCE_DEPS}"/*/BUILD-INFO; do
        [ -f "${info}" ] || continue
        echo "$(basename "$(dirname "${info}")") $(jq -r '.version' "${info}" 2>/dev/null || echo unknown)" >> ${BUILD_DIR}/resources/build_info.txt
    done
else
    echo "curl ${CURL_VERSION} (libnx TLS)" >> ${BUILD_DIR}/resources/build_info.txt
    echo "nghttp2 ${NGHTTP2_VERSION}" >> ${BUILD_DIR}/resources/build_info.txt
    echo "ffmpeg ${FFMPEG_VERSION}" >> ${BUILD_DIR}/resources/build_info.txt
fi
echo "" >> ${BUILD_DIR}/resources/build_info.txt
echo "=== DevkitPro Packages ===" >> ${BUILD_DIR}/resources/build_info.txt
dkp-pacman -Q >> ${BUILD_DIR}/resources/build_info.txt
