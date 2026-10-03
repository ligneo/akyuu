# Copyright (C) 2026, cenky <cenkkgl@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise valid and broken source companion archives through the release check."""
from io import BytesIO
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tarfile
import tempfile
import unittest


class SourceCompanionTests(unittest.TestCase):
    def fixture(self):
        data = b'corresponding source archive'
        digest = hashlib.sha256(data).hexdigest()
        inventory = json.dumps({'library': {'version': '1', 'binary': 'liblibrary1'}}).encode()
        return {
            'licenses/NOTICE': b'akyuu-0.1.0-beta.5-third-party-sources.tar',
            'licenses/licenses/GPL-3': b'common license text',
            'licenses/source-packages.json': inventory,
            'third-party-sources/README': b'akyuu-0.1.0-beta.5-third-party-sources.tar',
            'third-party-sources/source-packages.json': inventory,
            'third-party-sources/ubuntu/library_1.tar.xz': data,
            'third-party-sources/ubuntu/library_1.dsc':
                f'Source: library\nVersion: 1\nChecksums-Sha256:\n {digest} {len(data)} library_1.tar.xz\n'.encode(),
        }

    def run_check(self, files, manifest=True):
        if manifest:
            lines = [f'{hashlib.sha256(data).hexdigest()}  {name.removeprefix("third-party-sources/")}'
                     for name, data in files.items() if name.startswith('third-party-sources/')]
            files['third-party-sources/SHA256SUMS'] = ('\n'.join(lines) + '\n').encode()
        with tempfile.TemporaryDirectory() as work:
            archive = Path(work) / 'companion.tar'
            with tarfile.open(archive, 'w') as tar:
                for name, data in files.items():
                    member = tarfile.TarInfo(name)
                    member.size = len(data)
                    tar.addfile(member, BytesIO(data))
            return subprocess.run([sys.executable, str(Path(__file__).parents[1] / 'tools/verify-source-companion.py'),
                                   str(archive), '0.1.0-beta.5'], capture_output=True, text=True)

    def test_valid_archive(self):
        result = self.run_check(self.fixture())
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_stale_inventory(self):
        files = self.fixture()
        files['licenses/source-packages.json'] = b'{}'
        self.assertNotEqual(self.run_check(files).returncode, 0)

    def test_nested_licenses(self):
        files = self.fixture()
        files['licenses/licenses/NOTICE'] = files['licenses/NOTICE']
        self.assertNotEqual(self.run_check(files).returncode, 0)

    def test_stale_notice(self):
        files = self.fixture()
        files['licenses/NOTICE'] = b'akyuu-0.1.0-beta.4-third-party-sources.tar'
        self.assertNotEqual(self.run_check(files).returncode, 0)

    def test_unlisted_source(self):
        files = self.fixture()
        files['third-party-sources/SHA256SUMS'] = b''
        self.assertNotEqual(self.run_check(files, manifest=False).returncode, 0)

    def test_unreferenced_source(self):
        files = self.fixture()
        files['third-party-sources/ubuntu/stale-source.tar.xz'] = b'stale source'
        self.assertNotEqual(self.run_check(files).returncode, 0)

    def test_wrong_source_version(self):
        files = self.fixture()
        name = 'third-party-sources/ubuntu/library_1.dsc'
        files[name] = files[name].replace(b'Version: 1', b'Version: 2')
        self.assertNotEqual(self.run_check(files).returncode, 0)

    def test_wrong_descriptor_hash(self):
        files = self.fixture()
        files['third-party-sources/ubuntu/library_1.tar.xz'] = b'changed source archive'
        self.assertNotEqual(self.run_check(files).returncode, 0)


if __name__ == '__main__':
    unittest.main()
