#!/usr/bin/env bash
# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
[[ $# == 2 ]] || { echo "usage: $0 deb|rpm PACKAGE_DIR" >&2; exit 1; }
format=$1
packages=$(realpath "$2")
current=("$packages"/*."$format")
previous=("$packages/fixtures"/*."$format")
[[ ${#current[@]} == 1 && ${#previous[@]} == 1 ]]
install_package() {
	if [[ $format == deb ]]; then
		DEBIAN_FRONTEND=noninteractive apt-get install -y --reinstall "$1"
	elif command -v dnf >/dev/null; then
		dnf install -y "$1"
	else
		zypper --non-interactive --no-gpg-checks install "$1"
	fi
}
install_package "${previous[0]}"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export XDG_CONFIG_HOME="$work/config" XDG_DATA_HOME="$work/data"
export TMPDIR="$work/tmp"
mkdir -p "$TMPDIR"
mkdir -p "$XDG_DATA_HOME/akyuu/data"
marker="$XDG_DATA_HOME/akyuu/data/packaging-test-marker"
echo 'Preserve user data' > "$marker"
expected=$(sha256sum "$marker")
install_package "${current[0]}"
cp "$packages/diagnostics/deployment-$format" /opt/akyuu/bin/packaging-probe
chmod +x /opt/akyuu/bin/packaging-probe
unset LD_LIBRARY_PATH QT_PLUGIN_PATH QML_IMPORT_PATH QML2_IMPORT_PATH
QT_QPA_PLATFORM=offscreen /opt/akyuu/bin/packaging-probe --network
rm /opt/akyuu/bin/packaging-probe
set +e
xvfb-run -a timeout 10s /usr/bin/akyuu --debug
result=$?
set -e
[[ $result == 124 ]] || { echo "Installed application exited early ($result)." >&2; exit 1; }
if [[ $format == deb ]]; then
	dpkg-query -W -f='${Version}\n' akyuu
	apt-get remove -y akyuu
else
	rpm -q akyuu
	rpm -e akyuu
fi
[[ ! -e /usr/bin/akyuu && ! -e /opt/akyuu/bin/akyuu ]]
[[ $(sha256sum "$marker") == "$expected" ]]
echo 'Passed package install, fixture upgrade, startup, runtime and user-data preservation checks.'
