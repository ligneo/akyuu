#!/usr/bin/env bash
# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
[[ $# == 2 ]] || { echo "usage: $0 TAG PACKAGE_DIR" >&2; exit 1; }
root=$(cd "$(dirname "$0")/.." && pwd)
version=$(python3 "$root/tools/release-version.py")
tag=$1
[[ $tag == "v$version" ]] || { echo 'Tag does not match the source version.' >&2; exit 1; }
packages=$(realpath "$2")
assets=("akyuu-$version-x86_64.AppImage" "akyuu-$version-x86_64.deb" "akyuu-$version-x86_64.rpm"
        "akyuu-$version-x86_64-setup.exe" "akyuu-${version/-/}-1-x86_64.pkg.tar.zst"
        "akyuu-$version-source.tar.xz" "akyuu-$version-third-party-sources.tar" PKGBUILD)
for asset in "${assets[@]}"; do [[ -s $packages/$asset && ! -L $packages/$asset ]]; done
if gh release view "$tag" --repo ligneo/akyuu >/dev/null 2>&1; then
    echo 'A release already exists. Published files are immutable; inspect any existing draft manually.' >&2
    exit 1
fi
python3 "$root/tools/verify-source-companion.py" "$packages/akyuu-$version-third-party-sources.tar" "$version"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
(cd "$packages" && sha256sum "${assets[@]}" > SHA256SUMS)
cat > "$work/notes.md" <<EOF
See the [changelog](https://github.com/ligneo/akyuu/wiki/Changelog) and [installation guide](https://github.com/ligneo/akyuu/wiki/How-to-Compile).

Packages: Linux x86_64 AppImage, Arch, Debian/Ubuntu, Fedora/openSUSE, and Windows x64 installer. macOS is deferred. Native Linux runtime tests cover Ubuntu 24.04, Debian 13, Fedora 44 and openSUSE Leap 16.0; desktop and player integration testing remains separate. The Windows installer is unsigned.

Full application and corresponding third-party sources accompany the binaries. Downloads are checked against SHA256SUMS; installation remains with the user or package manager.
EOF
arguments=(--repo ligneo/akyuu --verify-tag --draft --title "Akyuu $version" --notes-file "$work/notes.md")
[[ $version != *-* ]] || arguments+=(--prerelease)
files=()
for asset in "${assets[@]}" SHA256SUMS; do files+=("$packages/$asset"); done
gh release create "$tag" "${files[@]}" "${arguments[@]}"
gh release download "$tag" --repo ligneo/akyuu --dir "$work/download"
(cd "$work/download" && sha256sum --check SHA256SUMS)
echo 'Draft created and all uploaded files verified. Publication is a separate maintainer action.'
