#!/usr/bin/env bash
# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Sets the version in src/akyuu/config.h, commits it and tags it, as Taiga's "Bump version to"
# commits do. Nothing is pushed.
#
# Usage: tools/bump-version.sh 0.2.0
#        tools/bump-version.sh 0.2.0-alpha

set -euo pipefail

version=${1:-}
if [[ ! $version =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)(-([0-9A-Za-z.]+))?$ ]]; then
	echo "usage: $0 MAJOR.MINOR.PATCH[-PRERELEASE]" >&2
	exit 1
fi
major=${BASH_REMATCH[1]} minor=${BASH_REMATCH[2]} patch=${BASH_REMATCH[3]} pre=${BASH_REMATCH[5]}

cd "$(git rev-parse --show-toplevel)"
config=src/akyuu/config.h

if [[ -n $(git status --porcelain --untracked-files=no) ]]; then
	echo "The working tree has uncommitted changes." >&2
	exit 1
fi
if git rev-parse -q --verify "refs/tags/v$version" >/dev/null; then
	echo "Tag v$version already exists." >&2
	exit 1
fi

sed -i -E \
	-e "s/^(#define AKYUU_VERSION_MAJOR) +[0-9]+$/\1 $major/" \
	-e "s/^(#define AKYUU_VERSION_MINOR) +[0-9]+$/\1 $minor/" \
	-e "s/^(#define AKYUU_VERSION_PATCH) +[0-9]+$/\1 $patch/" \
	-e "s/^(#define AKYUU_VERSION_PRE) +\"[^\"]*\"$/\1   \"$pre\"/" \
	"$config"

if git diff --quiet -- "$config"; then
	echo "$config already has version $version." >&2
	exit 1
fi

git commit -q -m "Bump version to $version" -- "$config"
git tag -a "v$version" -m "Akyuu $version"

echo "Committed and tagged v$version. To publish:"
echo "  git push origin HEAD v$version"
