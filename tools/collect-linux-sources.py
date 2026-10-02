#!/usr/bin/env python3
# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Collect exact Ubuntu source packages and notices for deployed system libraries."""
from pathlib import Path
import hashlib
import json
import shutil
import re
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
# Retain downloaded archives in a cache, but create fresh release inventories.
ubuntu = sources / 'ubuntu'
cache = sdk / 'cache' / 'ubuntu-sources'
cache.mkdir(parents=True, exist_ok=True)
if ubuntu.is_dir():
    for path in ubuntu.iterdir():
        if path.is_file():
            shutil.copyfile(path, cache / path.name)
    shutil.rmtree(ubuntu)
ubuntu.mkdir()
shutil.rmtree(licenses / 'ubuntu', ignore_errors=True)
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
        # Some packaged aliases and private helper libraries are absent from ldconfig.
        owners = subprocess.run(['dpkg-query', '-S', f'*/{library.name}'], text=True, capture_output=True)
        candidates = [Path(line.split(': ', 1)[1]) for line in owners.stdout.splitlines() if ': ' in line]
        candidates = [path for path in candidates if path.is_file() and path.name == library.name]
        if not candidates:
            raise RuntimeError(f'No system origin for bundled library: {library.name}')
        origin = candidates[0]
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

for package, data in sorted(packages.items()):
    subprocess.run(['apt-get', 'source', '--download-only', f'{package}={data["version"]}'], cwd=cache, check=True)
    descriptors = []
    for descriptor in cache.glob('*.dsc'):
        text = descriptor.read_text()
        if re.search(r'^Source: ' + re.escape(package) + r'$', text, re.M) and re.search(r'^Version: ' + re.escape(data['version']) + r'$', text, re.M):
            descriptors.append((descriptor, text))
    if len(descriptors) != 1:
        raise RuntimeError(f'Expected one source descriptor for {package}={data["version"]}')
    descriptor, text = descriptors[0]
    shutil.copyfile(descriptor, ubuntu / descriptor.name)
    checksums = re.search(r'^Checksums-Sha256:\n((?: .+\n)+)', text, re.M)
    if not checksums:
        raise RuntimeError(f'Missing source checksums: {descriptor}')
    for line in checksums[1].splitlines():
        expected, size, name = line.split()
        if Path(name).name != name:
            raise RuntimeError(f'Invalid source filename: {name}')
        path = cache / name
        with path.open('rb') as stream:
            if path.stat().st_size != int(size) or hashlib.file_digest(stream, 'sha256').hexdigest() != expected:
                raise RuntimeError(f'Source checksum mismatch: {name}')
        shutil.copyfile(path, ubuntu / name)
inventory = json.dumps(packages, indent=2, sort_keys=True) + '\n'
for directory in (sources, licenses):
    (directory / 'source-packages.json').write_text(inventory)
checksums = []
for path in sorted(sources.rglob('*')):
    if path.is_file() and path.name != 'SHA256SUMS':
        checksums.append(f'{hashlib.file_digest(path.open("rb"), "sha256").hexdigest()}  {path.relative_to(sources)}')
(sources / 'SHA256SUMS').write_text('\n'.join(checksums) + '\n')
print(f'Collected sources and notices for {len(packages)} bundled Ubuntu source packages.')
