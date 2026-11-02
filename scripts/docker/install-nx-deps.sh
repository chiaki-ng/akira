#!/bin/bash
set -euo pipefail

NX_DEPS_ROOT=/opt/nx-deps
PORTLIBS_PKGCONFIG=/opt/devkitpro/portlibs/switch/lib/pkgconfig

usage() {
    echo "Usage: $0 pool <dir>"
    echo "       $0 <library> <prefix>"
    exit 1
}

install_library() {
    local name=$1
    local source=$2
    local target="${NX_DEPS_ROOT}/${name}"

    if [ ! -d "${source}/lib" ]; then
        echo "nx-deps: ${source} has no lib directory" >&2
        exit 1
    fi

    local old_prefix
    old_prefix=$(sed -n 's/^prefix=//p' "${source}"/lib/pkgconfig/*.pc 2>/dev/null | head -n 1 || true)

    rm -rf "${target}"
    mkdir -p "${NX_DEPS_ROOT}"
    cp -a "${source}/." "${target}"

    if [ -n "${old_prefix}" ] && [ "${old_prefix}" != "${target}" ]; then
        local from="${old_prefix}"
        local to="${target}"
        if [ "${old_prefix##*/}" = "${name}" ]; then
            from="${old_prefix%/*}"
            to="${NX_DEPS_ROOT}"
        fi
        grep -rlZ --include='*.pc' --include='*.cmake' "${from}" "${target}" \
            | xargs -0 -r sed -i "s|${from}|${to}|g"
    fi

    mkdir -p "${PORTLIBS_PKGCONFIG}"
    for pc in "${target}"/lib/pkgconfig/*.pc; do
        if [ -e "${pc}" ]; then
            ln -sf "${pc}" "${PORTLIBS_PKGCONFIG}/"
        fi
    done

    echo "nx-deps: ${name} from ${source}"
}

if [ $# -ne 2 ]; then
    usage
fi

if [ "$1" = "pool" ]; then
    for library in "$2"/*/; do
        library="${library%/}"
        if [ -d "${library}/lib" ]; then
            install_library "${library##*/}" "${library}"
        fi
    done
else
    install_library "$1" "$2"
fi
