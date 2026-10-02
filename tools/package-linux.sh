#!/usr/bin/env bash
# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
[[ $# == 4 ]] || { echo "usage: $0 DEB|RPM BUNDLE VERSION OUTPUT_DIR" >&2; exit 1; }
format=$1
bundle=$(realpath "$2")
version=$3
output=$(realpath -m "$4")
root=$(cd "$(dirname "$0")/.." && pwd)
build=$(mktemp -d)
trap 'rm -rf "$build"' EXIT
cmake -S "$root/setup/linux" -B "$build" -DAKYUU_FORMAT="$format" \
	-DAKYUU_BUNDLE="$bundle" -DAKYUU_VERSION="$version" -DAKYUU_OUTPUT="$output"
(cd "$build" && cpack --config CPackConfig.cmake)
