#!/usr/bin/env bash
# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
[[ $# == 1 ]] || { echo "usage: $0 SDK_DIR" >&2; exit 1; }
sdk=$(realpath -m "$1")
mkdir -p "$sdk/cache" "$sdk/gcc" "$sdk/tools"
fetch() {
	local url=$1 name=$2 hash=$3
	if [[ ! -f $sdk/cache/$name ]]; then
		curl --fail --location --retry 3 "$url" -o "$sdk/cache/$name"
	fi
	echo "$hash  $sdk/cache/$name" | sha256sum --check --status
}
while read -r hash name; do
	package=${name%%-[0-9]*}
	fetch "https://archive.archlinux.org/packages/${package:0:1}/$package/$name" "$name" "$hash"
	tar -xf "$sdk/cache/$name" -C "$sdk/gcc" --exclude='.*'
done <<'PACKAGES'
1d79371ccf3138e3c172dd83b51560687b65ab824c1ad7e9cb41b7857ed9c55f gcc-16.2.1+r23+gd564253eb6c8-1-x86_64.pkg.tar.zst
7367dad49fc3229bde804816d412ab77306d433b1f92e4126b8b3ba3502c67d6 libgcc-16.2.1+r23+gd564253eb6c8-1-x86_64.pkg.tar.zst
15dc6bd2f3a2ee17fcd79a14325e9e0037722a592b2bdc55e1665d92112eaa51 libstdc++-16.2.1+r23+gd564253eb6c8-1-x86_64.pkg.tar.zst
dbeca7c4844e3112de98ba95f7c2b2618f4dd2b9c693c34076b7ba51ddf4611e gcc-libs-16.2.1+r23+gd564253eb6c8-1-x86_64.pkg.tar.zst
d9508ad848fd6c31473ac0925cf8c3ec0d2a71bbbb5c7e39274186338a285660 binutils-2.47-4-x86_64.pkg.tar.zst
PACKAGES
fetch https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage linuxdeploy-x86_64.AppImage 8aea8da0f7f7039d2a2cecb14657d752a222a5e1d3825caeef186c82f751cdd1
fetch https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage linuxdeploy-plugin-qt-x86_64.AppImage cfc1055b2b9dbc08412b579f20990b7b41a17b61beaa5847dc9477c96c9e9617
fetch https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64 runtime-x86_64 156f4bdbde9c52d01814600013e0a273f0118dc2de98975f3c8c63427ec79074
cp "$sdk/cache/"{linuxdeploy-x86_64.AppImage,linuxdeploy-plugin-qt-x86_64.AppImage,runtime-x86_64} "$sdk/tools/"
chmod +x "$sdk/tools/"*.AppImage
python3 -m venv "$sdk/aqt"
"$sdk/aqt/bin/pip" install aqtinstall==3.3.0
"$sdk/aqt/bin/aqt" install-qt linux desktop 6.11.2 linux_gcc_64 --outputdir "$sdk/Qt" -m qtmultimedia qtimageformats
# Keep Qt and GNU source/license payloads for the exact pinned runtime versions.
fetch https://github.com/ligneo/akyuu/releases/download/v0.1.0-beta.3/akyuu-0.1.0-beta.3-x86_64.AppImage baseline.AppImage a2aedc6f9e8dfca886176fd202445044211db7bf73b68d801a3bf3323917b027
fetch https://github.com/ligneo/akyuu/releases/download/v0.1.0-beta.3/akyuu-0.1.0-beta.3-third-party-sources.tar baseline-sources.tar f8f33db2e8dd807cc094b9820a34acc21ac8c88c14987a98e6318f21e22a7d95
mkdir -p "$sdk/baseline" "$sdk/sources"
chmod +x "$sdk/cache/baseline.AppImage"
(cd "$sdk/baseline" && "$sdk/cache/baseline.AppImage" --appimage-extract >/dev/null)
cp -a "$sdk/baseline/squashfs-root/usr/share/doc/akyuu" "$sdk/licenses"
tar -xf "$sdk/cache/baseline-sources.tar" -C "$sdk/sources"

mkdir -p "$sdk/templates"
cp "$sdk/licenses/NOTICE" "$sdk/templates/NOTICE"
cp "$sdk/sources/third-party-sources/README" "$sdk/templates/third-party-README"
