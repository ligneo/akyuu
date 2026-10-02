#!/usr/bin/env python3
# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Collect exact Ubuntu source packages and notices for deployed system libraries."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys

bundle, sdk = map(lambda value: Path(value).resolve(), sys.argv[1:])
sources = sdk / 'sources' / 'third-party-sources'
licenses = sdk / 'licenses'
paths = {}
for line in subprocess.check_output(['ldconfig', '-p'], text=True).splitlines():
    if '=>' in line:
        name, path = line.split('=>', 1)
        paths.setdefault(name.split()[0], Path(path.strip()))
packages = {}
for library in (bundle / 'usr').rglob('*.so*'):
    if library.is_symlink():
        continue
    relative = library.relative_to(bundle / 'usr')
    if relative.parts[0] == 'plugins' and (sdk / 'Qt/6.11.2/gcc_64' / relative).is_file():
        continue
    if library.name.startswith(('libQt6', 'libstdc++', 'libgcc_s', 'libicu', 'libav', 'libsw')):
        continue  # The pinned Qt/GNU/ICU/FFmpeg payload accompanies the SDK.
    origin = paths.get(library.name)
    if not origin:
        raise RuntimeError(f'No system origin for bundled library: {library.name}')
    package = None
    for path in {str(origin), str(origin.resolve()), str(origin).removeprefix('/usr')}:
        result = subprocess.run(['dpkg-query', '-S', path], text=True, capture_output=True)
        if result.returncode == 0:
            package = result.stdout.split(': ', 1)[0].splitlines()[0]
            break
    if package is None:
        raise RuntimeError(f'No package owner for {origin}')
    source, version, binary = subprocess.check_output([
        'dpkg-query', '-W', '-f=${source:Package}\t${source:Version}\t${binary:Package}', package
    ], text=True).split('\t')
    packages[source] = {'version': version, 'binary': binary}
    notice = Path('/usr/share/doc') / binary.split(':')[0] / 'copyright'
    if not notice.is_file():
        raise RuntimeError(f'Missing notice for {binary}')
    destination = licenses / 'ubuntu' / binary.split(':')[0]
    destination.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(notice, destination / 'copyright')

ubuntu = sources / 'ubuntu'
ubuntu.mkdir(parents=True, exist_ok=True)
for package, data in sorted(packages.items()):
    subprocess.run(['apt-get', 'source', '--download-only', f'{package}={data["version"]}'], cwd=ubuntu, check=True)
inventory = json.dumps(packages, indent=2, sort_keys=True) + '\n'
for directory in (sources, licenses):
    (directory / 'source-packages.json').write_text(inventory)
checksums = []
for path in sorted(sources.rglob('*')):
    if path.is_file() and path.name != 'SHA256SUMS':
        checksums.append(f'{hashlib.file_digest(path.open("rb"), "sha256").hexdigest()}  {path.relative_to(sources)}')
(sources / 'SHA256SUMS').write_text('\n'.join(checksums) + '\n')
print(f'Collected sources and notices for {len(packages)} bundled Ubuntu source packages.')
