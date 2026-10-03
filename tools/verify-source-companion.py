#!/usr/bin/env python3
# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Check the produced source companion before it reaches a release draft."""
import hashlib
import json
from pathlib import PurePosixPath
import re
import sys
import tarfile


def verify(path, version):
    with tarfile.open(path) as archive:
        members = {}
        for member in archive:
            name = PurePosixPath(member.name)
            if name.is_absolute() or '..' in name.parts or not name.parts:
                raise ValueError(f'Invalid archive path: {member.name}')
            if name.parts[0] not in ('licenses', 'third-party-sources'):
                raise ValueError(f'Unexpected archive root: {member.name}')
            # Common license texts live in licenses/licenses; duplicated metadata does not.
            if member.name in ('licenses/licenses/NOTICE', 'licenses/licenses/source-packages.json'):
                raise ValueError('The license tree must not contain a nested copy.')
            if member.name in members:
                raise ValueError(f'Duplicate archive member: {member.name}')
            members[member.name] = member

        def read(name):
            stream = archive.extractfile(name)
            if stream is None:
                raise ValueError(f'Expected an archive file: {name}')
            return stream.read()

        sources = json.loads(read('third-party-sources/source-packages.json'))
        notices = json.loads(read('licenses/source-packages.json'))
        if not sources or sources != notices:
            raise ValueError('Source and license inventories do not match.')
        for name in ('licenses/NOTICE', 'third-party-sources/README'):
            if f'akyuu-{version}-third-party-sources.tar' not in read(name).decode():
                raise ValueError(f'The release version is missing from {name}.')

        listed = set()
        for line in read('third-party-sources/SHA256SUMS').decode().splitlines():
            digest, name = line.split('  ', 1)
            if not re.fullmatch('[0-9a-f]{64}', digest) or name in listed:
                raise ValueError(f'Invalid checksum entry: {name}')
            listed.add(name)
            with archive.extractfile('third-party-sources/' + name) as stream:
                if hashlib.file_digest(stream, 'sha256').hexdigest() != digest:
                    raise ValueError(f'Checksum mismatch: {name}')
        payload = {name.removeprefix('third-party-sources/') for name, member in members.items()
                   if name.startswith('third-party-sources/') and member.isfile()
                   and name != 'third-party-sources/SHA256SUMS'}
        if listed != payload:
            raise ValueError('The checksum manifest does not cover the exact source payload.')

        descriptors = {}
        expected_ubuntu = set()
        for name in members:
            if not name.startswith('third-party-sources/ubuntu/') or not name.endswith('.dsc'):
                continue
            text = read(name).decode()
            source = re.search(r'^Source: (.+)$', text, re.M)
            release = re.search(r'^Version: (.+)$', text, re.M)
            if not source or not release:
                raise ValueError(f'Invalid source descriptor: {name}')
            package = source[1].strip()
            if package not in sources or package in descriptors or release[1].strip() != sources[package]['version']:
                raise ValueError(f'Source descriptor does not match the inventory: {name}')
            descriptors[package] = name
            expected_ubuntu.add(name)
            checksums = re.search(r'^Checksums-Sha256:\n((?: .+\n)+)', text, re.M)
            if not checksums:
                raise ValueError(f'Missing descriptor checksums: {name}')
            for line in checksums[1].splitlines():
                digest, size, filename = line.split()
                if PurePosixPath(filename).name != filename:
                    raise ValueError(f'Invalid source filename: {filename}')
                expected_ubuntu.add('third-party-sources/ubuntu/' + filename)
                member = archive.getmember('third-party-sources/ubuntu/' + filename)
                with archive.extractfile(member) as stream:
                    if member.size != int(size) or hashlib.file_digest(stream, 'sha256').hexdigest() != digest:
                        raise ValueError(f'Descriptor checksum mismatch: {filename}')
        if set(descriptors) != set(sources):
            raise ValueError('Source descriptors do not cover the exact inventory.')
        actual_ubuntu = {name for name, member in members.items()
                         if name.startswith('third-party-sources/ubuntu/') and member.isfile()}
        if actual_ubuntu != expected_ubuntu:
            raise ValueError('The Ubuntu payload contains missing or unreferenced source files.')
        print(f'Verified {len(sources)} source packages and {len(listed)} companion checksums.')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit('usage: verify-source-companion.py ARCHIVE VERSION')
    try:
        verify(sys.argv[1], sys.argv[2])
    except (ValueError, KeyError, TypeError, tarfile.TarError) as error:
        sys.exit(f'Source companion verification failed: {error}')
