#!/usr/bin/env python3
# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Archive committed application and submodule trees, with stable archive metadata."""
from io import BytesIO
from pathlib import Path
import subprocess
import sys
import tarfile

root = Path(__file__).resolve().parent.parent
output = Path(sys.argv[1]).resolve()
version = subprocess.check_output([sys.executable, str(root / "tools/release-version.py")], text=True).strip()

def git(repo, *arguments):
    return subprocess.check_output(["git", "-c", f"safe.directory={repo}", "-C", str(repo), *arguments])

def append(repo, prefix, archive):
    if git(repo, 'status', '--porcelain', '--untracked-files=normal').strip():
        raise RuntimeError(f'Source tree must be clean: {repo}')
    data = git(repo, 'archive', '--format=tar', 'HEAD')
    with tarfile.open(fileobj=BytesIO(data), mode='r:') as source:
        for member in source:
            member.name = f'{prefix}/{member.name}'
            member.uid = member.gid = 0
            member.uname = member.gname = ''
            archive.addfile(member, source.extractfile(member) if member.isfile() else None)
    for entry in git(repo, 'ls-tree', '-rz', 'HEAD').split(b'\0'):
        if not entry:
            continue
        metadata, path = entry.split(b'\t', 1)
        mode, kind, commit = metadata.split()
        if mode == b'160000':
            child = repo / path.decode()
            if git(child, 'rev-parse', 'HEAD').strip() != commit:
                raise RuntimeError(f'Submodule does not match its recorded commit: {child}')
            append(child, f'{prefix}/{path.decode()}', archive)

output.mkdir(parents=True, exist_ok=True)
with tarfile.open(output / f'akyuu-{version}-source.tar.xz', 'w:xz') as archive:
    append(root, f'akyuu-{version}', archive)
