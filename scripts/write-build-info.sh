#!/bin/bash
set -e

BUILD_DIR="${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
CORE_SOURCE_DIR="${AKIRA_CORE_DIR:-${BUILD_DIR}/library/libchiaki-core}"

# Generate build info file with all installed packages
echo "=== Generating build info ==="

CORE_COMMIT=$(git -C "${CORE_SOURCE_DIR}" rev-parse --short HEAD 2>/dev/null || echo "unknown")
CORE_VERSION=$(tr -d '[:space:]' < "${CORE_SOURCE_DIR}/VERSION.txt" 2>/dev/null || echo "unknown")
CURL_VERSION=$(jq -r '.version' ${NX_DEPS}/curl/BUILD-INFO 2>/dev/null || echo "unknown")
FFMPEG_VERSION=$(jq -r '.version' ${NX_DEPS}/ffmpeg/BUILD-INFO 2>/dev/null || echo "unknown")
NGHTTP2_VERSION=$(jq -r '.version' ${NX_DEPS}/nghttp2/BUILD-INFO 2>/dev/null || echo "unknown")

# Extract Akira version from cmake/version.cmake (channel/prerelease/commit composed below)
AKIRA_MAJOR=$(grep 'set(VERSION_MAJOR' ${BUILD_DIR}/cmake/version.cmake | sed 's/[^0-9]*//g')
AKIRA_MINOR=$(grep 'set(VERSION_MINOR' ${BUILD_DIR}/cmake/version.cmake | sed 's/[^0-9]*//g')
AKIRA_PATCH=$(grep 'set(VERSION_PATCH' ${BUILD_DIR}/cmake/version.cmake | sed 's/[^0-9]*//g')
AKIRA_CORE="${AKIRA_MAJOR}.${AKIRA_MINOR}.${AKIRA_PATCH}"

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
echo "curl ${CURL_VERSION} (libnx TLS)" >> ${BUILD_DIR}/resources/build_info.txt
echo "nghttp2 ${NGHTTP2_VERSION}" >> ${BUILD_DIR}/resources/build_info.txt
echo "ffmpeg ${FFMPEG_VERSION}" >> ${BUILD_DIR}/resources/build_info.txt
echo "" >> ${BUILD_DIR}/resources/build_info.txt
echo "=== DevkitPro Packages ===" >> ${BUILD_DIR}/resources/build_info.txt
dkp-pacman -Q >> ${BUILD_DIR}/resources/build_info.txt
