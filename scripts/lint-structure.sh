#!/bin/bash
set -euo pipefail

REPOSITORY="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${REPOSITORY}"

SINGLETON_SEAMS_STILL_ALLOWED=()


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

is_borealis_screen() {
    case "$1" in
        */views/*) return 0 ;;
        *) return 1 ;;
    esac
}

is_arisa_screen() {
    case "$1" in
        source/app/arisa/* | include/app/arisa/* | source/*/ui/* | include/*/ui/*) return 0 ;;
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
    if ! is_borealis_screen "${file}"; then
        report "borealis outside views/" "${file}" \
            "$(code_lines "${file}" '#[[:space:]]*include[[:space:]]*[<"]borealis|brls::')"
        report "views/ header outside views/" "${file}" \
            "$(code_lines "${file}" '#[[:space:]]*include[[:space:]]*"[^"]*views/')"
    fi
    if ! is_arisa_screen "${file}"; then
        report "arisa outside app/arisa/ and ui/" "${file}" \
            "$(code_lines "${file}" '#[[:space:]]*include[[:space:]]*[<"]arisa/')"
        report "ui/ header outside app/arisa/ and ui/" "${file}" \
            "$(code_lines "${file}" '#[[:space:]]*include[[:space:]]*"([^"]*/ui/|app/arisa/)')"
    fi
    if ! is_view_or_app "${file}" || is_arisa_screen "${file}"; then
        report "ViewServices outside views/ and the borealis app" "${file}" \
            "$(code_lines "${file}" '#[[:space:]]*include[[:space:]]*"app/view_services\.hpp"|\bViewServices\b')"
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
