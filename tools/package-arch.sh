#!/usr/bin/env bash
# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
[[ $# == 2 ]] || { echo "usage: $0 SOURCE_ARCHIVE OUTPUT_DIR" >&2; exit 1; }
root=$(cd "$(dirname "$0")/.." && pwd)
version=$(python3 "$root/tools/release-version.py")
archive=$(realpath "$1")
output=$(realpath -m "$2")
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cp "$archive" "$work/"
mkdir -p "$output"
cat > "$work/PKGBUILD" <<EOF
pkgname=akyuu
pkgver=${version/-/}
pkgrel=1
pkgdesc='Anime list tracker with player and browser detection'
arch=('x86_64')
url='https://github.com/ligneo/akyuu'
license=('GPL-3.0-or-later')
depends=('qt6-base' 'qt6-multimedia' 'qt6-svg' 'systemd-libs' 'hicolor-icon-theme')
makedepends=('cmake' 'ninja' 'gcc')
source=('https://github.com/ligneo/akyuu/releases/download/v$version/$(basename "$archive")')
sha256sums=('$(sha256sum "$archive" | cut -d' ' -f1)')
build() {
  cmake -S "akyuu-$version" -B build -G Ninja -DCMAKE_BUILD_TYPE=None \\
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="\$srcdir/build/bin" -DCMAKE_INSTALL_PREFIX=/usr \\
    -DAKYUU_PORTABLE=OFF -DAKYUU_PACKAGE_FORMAT=arch -DAKYUU_BUILD_TESTS=ON
  cmake --build build --parallel "\${BUILD_JOBS:-2}"
}
check() { ctest --test-dir build --output-on-failure; }
package() {
  DESTDIR="\$pkgdir" cmake --install build
  install -Dm644 "akyuu-$version/LICENSE" "\$pkgdir/usr/share/licenses/akyuu/LICENSE"
}
EOF
(cd "$work" && makepkg --cleanbuild --force --noconfirm)
cp "$work/"*.pkg.tar.zst "$work/PKGBUILD" "$output/"
