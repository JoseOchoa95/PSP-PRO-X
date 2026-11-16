"""Offline cases for the production catalog used by the original PSP UI."""
from pathlib import Path
import copy
import hashlib
import json
import os
import runpy
import subprocess
import tempfile
import unittest
from test_update_contract import fixture

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT/'artifacts/phase3/native-catalog'


@unittest.skipUnless(os.environ.get('PROX_RUN_CONTRACT_TESTS') == '1', 'requires offline native environment')
class NativeCatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        WORK.mkdir(parents=True, exist_ok=True)
        tls = ROOT/'artifacts/phase3/https-native/mbedtls'
        if not (tls/'library/libmbedcrypto.a').exists():
            from test_update_https import prepare
            prepare()
        command = ['cc', '-std=c99', '-Wall', '-Wextra', '-Werror',
                   '-DMBEDTLS_CONFIG_FILE="prox_mbedtls_config.h"',
                   '-I'+str(ROOT/'firmware/Updater'), '-I'+str(ROOT/'firmware/Common'),
                   '-I'+str(tls/'include'), str(ROOT/'tests/native/test_native_catalog.c')]
        command += [str(ROOT/'firmware/Updater'/p) for p in ('catalog.c', 'json.c', 'contract.c', 'policy.c')]
        command += ['-L'+str(tls/'library'), '-lmbedcrypto', '-o', str(WORK/'catalog-test')]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def setUp(self):
        self.release, self.manifest, _, _ = fixture()

    def check(self, expected, releases=None, manifest=None, mode='normal', requests=None):
        content = json.dumps(self.manifest if manifest is None else manifest).encode()
        r = copy.deepcopy(self.release)
        r['assets'][0]['size'] = len(content)
        if releases is None:
            releases = [r]
        with tempfile.TemporaryDirectory(dir=WORK) as name:
            directory = Path(name)
            (directory/'catalog.json').write_bytes(releases if isinstance(releases, bytes) else json.dumps(releases).encode())
            (directory/'manifest.json').write_bytes(content)
            result = subprocess.run([str(WORK/'catalog-test'), name, mode], capture_output=True, text=True)
        self.assertEqual(result.returncode, expected, result.stdout+result.stderr)
        if requests is not None:
            self.assertIn('requests='+str(requests)+' ', result.stdout)
        return result.stdout

    def test_valid_newer_contract(self):
        self.assertIn('version=0.4.0', self.check(0, requests=2))

    def test_older_release_never_downloaded(self):
        self.release['tag_name'] = 'v0.3.1'
        self.check(2, requests=1)

    def test_equal_version_never_downloaded(self):
        self.release['tag_name'] = 'v'+runpy.run_path(str(ROOT/'scripts/product.py'))['VERSION']
        self.check(2, requests=1)

    def test_running_client_version_mismatch(self):
        self.check(1,mode='client-mismatch',requests=0)

    def test_draft_flag_without_beta_name(self):
        self.release['draft'] = True
        self.check(2, requests=1)

    def test_prerelease_flag_without_beta_name(self):
        self.release['prerelease'] = True
        self.check(2, requests=1)

    def test_unsupported_model_is_error_not_current(self):
        self.manifest['supported_models'] = [0]
        self.check(1, requests=2)

    def test_future_minimum_client_is_error(self):
        self.manifest['minimum_updater_version'] = '0.4.0'
        self.check(1, requests=2)

    def test_manifest_digest_mismatch(self):
        self.release['assets'][0]['digest'] = 'sha256:'+'0'*64
        self.check(1, requests=2)

    def test_manifest_size_mismatch(self):
        self.check(1, releases=[self.release], requests=2)

    def test_insecure_asset_is_rejected(self):
        self.release['assets'][0]['browser_download_url'] = 'http://github.com/unsafe'
        self.check(1, requests=1)

    def test_duplicate_version_is_ambiguous(self):
        self.check(1, releases=[self.release, self.release], requests=1)

    def test_malformed_json(self):
        self.check(1, releases=b'[broken', requests=1)

    def test_http_error_not_current(self):
        self.check(1, mode='http', requests=1)

    def test_network_error_not_current(self):
        self.check(1, mode='network', requests=1)

    def test_cancel_before_request(self):
        self.check(1, mode='cancel', requests=0)

    def test_incomplete_pagination_not_current(self):
        self.release['prerelease'] = True
        self.check(1, releases=[self.release]*10, requests=5)
