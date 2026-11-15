#!/bin/bash
set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <native-deps-ounce pool>" >&2
    exit 1
fi

POOL=$1
PREFIX="${DEVKITPRO:?}/portlibs/switch2"
MANIFESTS="${PREFIX}/share/ounce-deps"

install_library() {
    local source=$1
    local name=${source##*/}
    local old_prefix

    [ -d "${source}/lib" ] || { echo "ounce-deps: ${source} has no lib directory" >&2; exit 1; }
    old_prefix=$(sed -n 's/^prefix=//p' "${source}"/lib/pkgconfig/*.pc 2>/dev/null | head -n 1 || true)

    for part in include lib share; do
        if [ -d "${source}/${part}" ]; then
            mkdir -p "${PREFIX}/${part}"
            cp -a "${source}/${part}/." "${PREFIX}/${part}/"
        fi
    done

    if [ -n "${old_prefix}" ] && [ "${old_prefix}" != "${PREFIX}" ]; then
        grep -rlZ --include='*.pc' --include='*.cmake' "${old_prefix}" "${PREFIX}/lib" "${PREFIX}/share" 2>/dev/null \
            | xargs -0 -r sed -i "s|${old_prefix}|${PREFIX}|g"
    fi

    mkdir -p "${MANIFESTS}/${name}"
    [ -f "${source}/BUILD-INFO" ] && cp "${source}/BUILD-INFO" "${MANIFESTS}/${name}/BUILD-INFO"
    echo "ounce-deps: ${name}"
}

for library in "${POOL}"/*/; do
    install_library "${library%/}"
done
