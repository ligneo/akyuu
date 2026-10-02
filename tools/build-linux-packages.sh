#!/usr/bin/env bash
# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
[[ $# == 1 ]] || { echo "usage: $0 OUTPUT_DIR" >&2; exit 1; }
: "${SDK_DIR:?Set SDK_DIR to the prepared Linux SDK}"
root=$(cd "$(dirname "$0")/.." && pwd)
output=$(realpath -m "$1")
mkdir -p "$output"
version=$(python3 "$root/tools/release-version.py")
python3 "$root/tools/source-bundle.py" "$output"
python3 - "$SDK_DIR" "$version" <<'PYTHON'
from pathlib import Path
import sys
sdk, version = Path(sys.argv[1]), sys.argv[2]
for template, path in (("NOTICE", sdk / "licenses/NOTICE"),
                       ("third-party-README", sdk / "sources/third-party-sources/README")):
    path.write_text((sdk / "templates" / template).read_text().replace("0.1.0-beta.3", version))
PYTHON
export LINUXDEPLOY="$SDK_DIR/tools/linuxdeploy-x86_64.AppImage"
export LINUXDEPLOY_QT="$SDK_DIR/tools/linuxdeploy-plugin-qt-x86_64.AppImage"
export LDAI_RUNTIME_FILE="$SDK_DIR/tools/runtime-x86_64"
export QMAKE="$SDK_DIR/Qt/6.11.2/gcc_64/bin/qmake"
export GCC_RUNTIME_DIR="$SDK_DIR/gcc/usr/lib"
export APPIMAGE_EXTRACT_AND_RUN=1
export CPLUS_INCLUDE_PATH=/usr/include/x86_64-linux-gnu
export C_INCLUDE_PATH=/usr/include/x86_64-linux-gnu
export LIBRARY_PATH=/usr/lib/x86_64-linux-gnu
export LD_LIBRARY_PATH="$GCC_RUNTIME_DIR:$SDK_DIR/Qt/6.11.2/gcc_64/lib"
export PATH="$SDK_DIR/gcc/usr/bin:$PATH"
work=${AKYUU_PACKAGE_WORK:-$(mktemp -d)}
mkdir -p "$work"
if [[ -z ${AKYUU_PACKAGE_WORK:-} ]]; then trap 'rm -rf "$work"' EXIT; fi
for format in appimage deb rpm; do
	build="$work/build"
	cmake -S "$root" -B "$build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_CXX_COMPILER="$SDK_DIR/gcc/usr/bin/g++" \
		-DCMAKE_C_COMPILER="$SDK_DIR/gcc/usr/bin/gcc" \
		-DCMAKE_PREFIX_PATH="$SDK_DIR/Qt/6.11.2/gcc_64" \
		-DCMAKE_C_FLAGS=-B/usr/lib/x86_64-linux-gnu/ \
		-DCMAKE_CXX_FLAGS=-B/usr/lib/x86_64-linux-gnu/ \
		-DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$build/bin" \
		-DCMAKE_INSTALL_PREFIX=/usr -DAKYUU_PORTABLE=OFF \
		-DAKYUU_PACKAGE_FORMAT="$format" -DAKYUU_BUILD_TESTS=ON
	cmake --build "$build" --parallel "${BUILD_JOBS:-2}"
	ctest --test-dir "$build" --output-on-failure
	"$root/tools/package-appimage.sh" "$build" "$work/$format.AppDir" "$SDK_DIR/licenses" "$output"
	if [[ $format != appimage ]]; then
		"$root/tools/package-linux.sh" "${format^^}" "$work/$format.AppDir" "$version" "$output"
		"$root/tools/package-linux.sh" "${format^^}" "$SDK_DIR/baseline/squashfs-root" "0.0.1" "$output/fixtures"
	fi
done

cp -a "$SDK_DIR/licenses" "$SDK_DIR/sources/licenses"
tar -cf "$output/akyuu-$version-third-party-sources.tar" -C "$SDK_DIR/sources" licenses third-party-sources
