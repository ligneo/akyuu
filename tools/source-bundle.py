#!/usr/bin/env python3
# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path
import subprocess
import sys
import tarfile

root = Path(__file__).resolve().parent.parent
output = Path(sys.argv[1]).resolve()
version = subprocess.check_output([sys.executable, str(root / "tools/release-version.py")], text=True).strip()

def files(repo):
    names = subprocess.check_output(["git", "-c", f"safe.directory={repo}", "-C", str(repo), "ls-files", "-z", "--cached", "--others", "--exclude-standard"]).split(b"\0")
    for raw in names:
        if not raw:
            continue
        path = repo / raw.decode()
        if path.is_dir():
            yield from files(path)
        elif path.is_file():
            yield path

output.mkdir(parents=True, exist_ok=True)
with tarfile.open(output / f"akyuu-{version}-source.tar.xz", "w:xz") as archive:
    for path in files(root):
        archive.add(path, arcname=f"akyuu-{version}/{path.relative_to(root)}", recursive=False)
