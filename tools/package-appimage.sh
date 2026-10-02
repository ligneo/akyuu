#!/usr/bin/env bash
# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail
if [[ $# != 4 ]]; then
	echo "usage: $0 BUILD_DIR APPDIR LICENSE_DIR OUTPUT_DIR" >&2
	exit 1
fi
build=$(realpath "$1")
appdir=$(realpath -m "$2")
licenses=$(realpath "$3")
output=$(realpath -m "$4")
: "${LINUXDEPLOY:?Set LINUXDEPLOY to a verified linuxdeploy executable}"
: "${QMAKE:?Set QMAKE to the Qt SDK qmake executable}"
: "${LINUXDEPLOY_QT:?Set LINUXDEPLOY_QT to the verified Qt deployment plugin}"
: "${GCC_RUNTIME_DIR:?Set GCC_RUNTIME_DIR to the compiler runtime library directory}"
: "${LDAI_RUNTIME_FILE:?Set LDAI_RUNTIME_FILE to a verified AppImage runtime}"
[[ ! -e $appdir ]] || { echo "APPDIR must not exist." >&2; exit 1; }
format=$(sed -n 's/^AKYUU_PACKAGE_FORMAT:STRING=//p' "$build/CMakeCache.txt")
[[ $format == appimage || $format == deb || $format == rpm ]]
grep -q '^AKYUU_PORTABLE:BOOL=OFF$' "$build/CMakeCache.txt"
grep -q '^CMAKE_INSTALL_PREFIX:PATH=/usr$' "$build/CMakeCache.txt"

root=$(cd "$(dirname "$0")/.." && pwd)
version=$(python3 - "$root/src/akyuu/config.h" <<'PY'
import re, sys
text = open(sys.argv[1]).read()
parts = [re.search(r'^#define AKYUU_VERSION_' + name + r' +(\d+)$', text, re.M)[1]
         for name in ('MAJOR', 'MINOR', 'PATCH')]
pre = re.search(r'^#define AKYUU_VERSION_PRE +"([^"]*)"$', text, re.M)[1]
print('.'.join(parts) + ('-' + pre if pre else ''))
PY
)
architecture=$(uname -m)
[[ $architecture == x86_64 ]] || { echo "The prepared Linux SDK supports x86_64 only." >&2; exit 1; }

cmake --build "$build" --parallel "${BUILD_JOBS:-2}"
DESTDIR="$appdir" cmake --install "$build"
mkdir -p "$appdir/usr/share/doc/akyuu" "$output"
cp "$root/LICENSE" "$appdir/usr/share/doc/akyuu/LICENSE"
cp -a "$licenses/." "$appdir/usr/share/doc/akyuu/"
if [[ -f $build/tests/akyuu-deployment-tests ]]; then
	cp "$build/tests/akyuu-deployment-tests" "$appdir/usr/bin/akyuu-deployment-tests"
fi
export QMAKE LDAI_RUNTIME_FILE APPIMAGE_EXTRACT_AND_RUN=1
export EXTRA_QT_MODULES=svg
export EXTRA_PLATFORM_PLUGINS='libqwayland.so;libqoffscreen.so'
export LD_LIBRARY_PATH="$GCC_RUNTIME_DIR:$(dirname "$QMAKE")/../lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LDAI_VERSION="$version"
export LDAI_OUTPUT="$output/akyuu-$version-$architecture.AppImage"
"$LINUXDEPLOY" --appdir "$appdir" \
	--library "$GCC_RUNTIME_DIR/libstdc++.so.6" \
	--library "$GCC_RUNTIME_DIR/libgcc_s.so.1" \
	--desktop-file "$root/src/resources/io.github.ligneo.Akyuu.desktop" \
	--icon-file "$root/src/resources/icons/akyuu.png" \
	--icon-filename io.github.ligneo.Akyuu
sql_plugins=$("$QMAKE" -query QT_INSTALL_PLUGINS)/sqldrivers
qt_arguments=(--appdir "$appdir")
for driver in "$sql_plugins"/libqsql*.so; do
	[[ $(basename "$driver") == libqsqlite.so ]] && continue
	qt_arguments+=(--exclude-library "$(basename "$driver")")
done
"$LINUXDEPLOY_QT" "${qt_arguments[@]}"
# Keep vendor-neutral OpenGL entry points; graphics drivers come from the host.
for name in libOpenGL.so.0 libGLdispatch.so.0; do
	library=$(ldconfig -p | awk -v name="$name" '$1 == name && !found { print $NF; found=1 }')
	[[ -n $library ]]
	cp -L "$library" "$appdir/usr/lib/$name"
done
if [[ -n ${SDK_DIR:-} ]]; then
	python3 "$root/tools/collect-linux-sources.py" "$appdir" "$SDK_DIR"
	cp -a "$licenses/." "$appdir/usr/share/doc/akyuu/"
fi
if [[ -f $appdir/usr/bin/akyuu-deployment-tests ]]; then
	mkdir -p "$output/diagnostics"
	cp "$appdir/usr/bin/akyuu-deployment-tests" "$output/diagnostics/deployment-$format"
	rm "$appdir/usr/bin/akyuu-deployment-tests"
fi
if [[ $format == appimage ]]; then
	"$LINUXDEPLOY" --appdir "$appdir" --output appimage
	[[ -s $LDAI_OUTPUT ]]
	echo "$LDAI_OUTPUT"
else
	ln -s usr/bin/akyuu "$appdir/AppRun"
	echo "$appdir"
fi
