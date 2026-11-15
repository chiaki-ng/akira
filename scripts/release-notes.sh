#!/bin/bash
set -euo pipefail

DEVKIT="$1"
IMAGE="$2"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "## Built from"
echo
echo "| Component | Version |"
echo "| --- | --- |"
echo "| akira-ounce | \`$(git -C "$ROOT" rev-parse HEAD)\` |"
while read -r name rest; do
    [ -n "$name" ] && echo "| ${name} | \`${rest}\` |"
done < "${DEVKIT}/ounce-devkit-version.txt"
echo

echo "## Submodules"
echo
echo "| Path | Repository | Branch | Commit |"
echo "| --- | --- | --- | --- |"
git -C "$ROOT" config -f .gitmodules --get-regexp '^submodule\..*\.path$' | while read -r key path; do
    name="${key#submodule.}"
    name="${name%.path}"
    url="$(git -C "$ROOT" config -f .gitmodules "submodule.${name}.url")"
    branch="$(git -C "$ROOT" config -f .gitmodules "submodule.${name}.branch" || echo "-")"
    commit="$(git -C "$ROOT" rev-parse "HEAD:${path}")"
    echo "| ${path} | ${url} | ${branch} | \`${commit}\` |"
done
echo

echo "## Native libraries"
echo
echo "| Library | Version | Source |"
echo "| --- | --- | --- |"
for toml in "${ROOT}"/library/native-deps/deps/*/ounce.toml; do
    lib="$(basename "$(dirname "$toml")")"
    grep -q "\"${lib}\"" "${ROOT}/library/native-deps/config/ounce.toml" || continue
    version="$(sed -n 's/^version = "\(.*\)"/\1/p' "$toml")"
    repo="$(sed -n 's/^repo = "\(.*\)"/\1/p' "$toml")"
    if [ -n "$repo" ]; then
        source="${repo} \`$(sed -n 's/^commit = "\(.*\)"/\1/p' "$toml")\`"
    else
        url="$(sed -n 's/^url = "\(.*\)"/\1/p' "$toml")"
        source="${url//\{version\}/$version} sha256 \`$(sed -n 's/^sha256 = "\(.*\)"/\1/p' "$toml")\`"
    fi
    echo "| ${lib} | ${version} | ${source} |"
done

echo
echo "## devkitPro packages"
echo
echo '```'
docker run --rm "$IMAGE" bash -c 'dkp-pacman -Q 2>/dev/null || pacman -Q'
echo '```'
