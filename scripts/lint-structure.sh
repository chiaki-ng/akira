#!/bin/bash
set -euo pipefail

REPOSITORY="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPOSITORY}"

RENDERERS_DRAW_ON_THE_BOREALIS_GPU_DEVICE_UNTIL_A1=(
    include/platform/video/deko3d/renderer.hpp
    source/platform/video/deko3d/renderer.cpp
    include/platform/video/vulkan/renderer.hpp
    source/platform/video/vulkan/renderer.cpp
)
OUNCE_FACTORY_PICKS_THE_BOREALIS_RENDERER_BACKEND=(
    include/platform/factories/ounce/factory.hpp
    source/platform/factories/ounce/factory.cpp
)
PAD_PATHS_RUMBLE_THROUGH_THE_BOREALIS_INPUT_MANAGER=(
    source/platform/pads/extended/pad_path.cpp
    source/platform/pads/extended/pad_path_hos.cpp
    source/platform/pads/extended/pad_path_mc.cpp
    source/platform/pads/hid/hid_pad.cpp
)
UPDATE_MANAGER_PARSES_JSON_WITH_THE_BOREALIS_NLOHMANN=(
    source/platform/updates/github/update_manager.cpp
)
RENDERERS_READ_THE_ACTIVE_THEME_UNTIL_A1=(
    source/platform/video/deko3d/renderer.cpp
    source/platform/video/vulkan/renderer.cpp
)
SINGLETON_SEAMS_STILL_ALLOWED=()

BOREALIS_ALLOWED=(
    "${RENDERERS_DRAW_ON_THE_BOREALIS_GPU_DEVICE_UNTIL_A1[@]}"
    "${OUNCE_FACTORY_PICKS_THE_BOREALIS_RENDERER_BACKEND[@]}"
    "${PAD_PATHS_RUMBLE_THROUGH_THE_BOREALIS_INPUT_MANAGER[@]}"
    "${UPDATE_MANAGER_PARSES_JSON_WITH_THE_BOREALIS_NLOHMANN[@]}"
)
VIEW_HEADERS_ALLOWED=(
    "${RENDERERS_READ_THE_ACTIVE_THEME_UNTIL_A1[@]}"
)

violations=0

source_files() {
    find source include -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.c' \) | sort
}

is_view_or_app() {
    case "$1" in
        source/app/* | include/app/* | */views/*) return 0 ;;
        *) return 1 ;;
    esac
}

is_listed() {
    local file="$1"
    shift
    local listed
    for listed in "$@"; do
        [ "${listed}" = "${file}" ] && return 0
    done
    return 1
}

code_lines() {
    grep -nE "$2" "$1" | grep -vE '^[0-9]+:[[:space:]]*(//|/\*|\*)' || true
}

report() {
    local rule="$1"
    local file="$2"
    local found="$3"
    while IFS= read -r line; do
        [ -n "${line}" ] || continue
        echo "${rule}: ${file}:${line}"
        violations=$((violations + 1))
    done <<< "${found}"
}

while IFS= read -r file; do
    if ! is_view_or_app "${file}"; then
        if ! is_listed "${file}" "${BOREALIS_ALLOWED[@]}"; then
            report "borealis outside app/ and views/" "${file}" \
                "$(code_lines "${file}" '#[[:space:]]*include[[:space:]]*[<"]borealis|brls::')"
        fi
        report "ViewServices outside app/ and views/" "${file}" \
            "$(code_lines "${file}" '#[[:space:]]*include[[:space:]]*"app/view_services\.hpp"|\bViewServices\b')"
        if ! is_listed "${file}" "${VIEW_HEADERS_ALLOWED[@]}"; then
            report "views/ header in a non-view file" "${file}" \
                "$(code_lines "${file}" '#[[:space:]]*include[[:space:]]*"[^"]*views/')"
        fi
    fi
    report "ChiakiCore::current locator" "${file}" \
        "$(code_lines "${file}" '\bChiakiCore::current\b')"
    singletons="$(code_lines "${file}" '\b(getInstance|GetInstance|instance)[[:space:]]*\([[:space:]]*\)')"
    if [ -n "${singletons}" ] && ! is_listed "${file}" "${SINGLETON_SEAMS_STILL_ALLOWED[@]+"${SINGLETON_SEAMS_STILL_ALLOWED[@]}"}"; then
        report "singleton accessor" "${file}" "${singletons}"
    fi
done < <(source_files)

if [ "${violations}" -ne 0 ]; then
    echo "lint-structure: ${violations} violation(s)" >&2
    exit 1
fi
echo "lint-structure: clean"
