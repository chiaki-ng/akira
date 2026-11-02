#!/bin/bash
set -eo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <uam source>"
    exit 1
fi

source /opt/devkitpro/switchvars.sh

work=$(mktemp -d)
trap 'rm -rf "${work}"' EXIT
cp -a "$1/." "${work}"
cd "${work}"

echo "option('build_as_library', type : 'boolean', value : false)" > meson_options.txt
sed -i "/static_library/,/^[[:space:]]*)/{s/include_directories: uam_incs$/include_directories: uam_incs, install: true/}" meson.build

/opt/devkitpro/meson-cross.sh switch crossfile_dkp build -Dbuild_as_library=true
meson compile -C build
meson install -C build
cp source/uam.h "${PORTLIBS_PREFIX}/include/"

echo "uam: from $1"
