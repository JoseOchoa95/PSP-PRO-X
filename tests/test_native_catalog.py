"""Offline cases for the production catalog used by the original PSP UI."""
from pathlib import Path
import copy
import hashlib
import json
import os
import subprocess
import sys
import tempfile
import unittest
from test_update_contract import LEGACY_CLIENT_VERSION, fixture, fixture_product_header

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT/'artifacts/phase3/native-catalog'
sys.path.insert(0, str(ROOT/'scripts'))
from dedicated_update import release_manifest, render_fixture


@unittest.skipUnless(os.environ.get('PROX_RUN_CONTRACT_TESTS') == '1', 'requires offline native environment')
class NativeCatalogTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        WORK.mkdir(parents=True, exist_ok=True)
        tls = ROOT/'artifacts/phase3/https-native/mbedtls'
        if not (tls/'library/libmbedcrypto.a').exists():
            from test_update_https import prepare
            prepare()
        client = fixture_product_header(WORK/'client-fixture', LEGACY_CLIENT_VERSION)
        command = ['cc', '-std=c99', '-Wall', '-Wextra', '-Werror',
                   '-DPROX_NATIVE_DEDICATED_PREVIEW=1',
                   '-DMBEDTLS_CONFIG_FILE="prox_mbedtls_config.h"',
                   '-I'+str(client),
                   '-I'+str(ROOT/'firmware/Updater'), '-I'+str(ROOT/'firmware/Common'),
                   '-I'+str(ROOT/'tests/native/loader_sdk'),
                   '-I'+str(ROOT/'firmware/tools/nativeinstaller'),
                   '-I'+str(tls/'include'), str(ROOT/'tests/native/test_native_catalog.c')]
        command += [str(ROOT/'firmware/Updater'/p) for p in ('catalog.c', 'json.c', 'contract.c', 'policy.c',
                    'native_dedicated_stage.c', 'dedicated_update.c', 'dedicated_job.c', 'native_payload.c', 'stage.c')]
        command += [str(ROOT/'firmware/tools/nativeinstaller/dedicated_writer.c')]
        command += ['-L'+str(tls/'library'), '-lmbedcrypto', '-lz', '-o', str(WORK/'catalog-test')]
        result = subprocess.run(command, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def setUp(self):
        self.release, self.manifest, _, _ = fixture()

    def check(self, expected, releases=None, manifest=None, mode='normal', requests=None, package=None):
        content = json.dumps(self.manifest if manifest is None else manifest).encode()
        r = copy.deepcopy(self.release)
        r['assets'][0]['size'] = len(content)
        if releases is None:
            releases = [r]
        with tempfile.TemporaryDirectory(dir=WORK) as name:
            directory = Path(name)
            (directory/'catalog.json').write_bytes(releases if isinstance(releases, bytes) else json.dumps(releases).encode())
            (directory/'manifest.json').write_bytes(content)
            if package is not None:
                (directory/'package.pbp').write_bytes(package)
                (directory/'ms0:/PROXDIAG').mkdir(parents=True)
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
        self.release['tag_name'] = 'v'+LEGACY_CLIENT_VERSION
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

    def dedicated_offer(self):
        package, identity = render_fixture()
        self.manifest = release_manifest(identity)
        self.manifest['minimum_updater_version'] = LEGACY_CLIENT_VERSION
        self.release['tag_name'] = self.manifest['tag']
        base = 'https://github.com/JoseOchoa95/PSP-PRO-X/releases/download/'+self.manifest['tag']
        self.release['assets'] = [
            dict(name='pro-x-dedicated-update.json', size=1, browser_download_url=base+'/pro-x-dedicated-update.json'),
            dict(name='EBOOT.PBP', size=len(package), browser_download_url=base+'/EBOOT.PBP',
                 digest='sha256:'+identity['sha256']),
        ]
        return package

    def test_dedicated_discovery_without_package_request(self):
        self.dedicated_offer()
        self.check(0, mode='dedicated', requests=2)
        self.check(2, requests=1)

    def test_dedicated_previews_require_valid_catalog(self):
        self.dedicated_offer()
        self.release['prerelease'] = True
        self.check(2, mode='dedicated', requests=1)
        self.release['prerelease'] = False
        self.manifest['minimum_engine_revision'] = 2
        self.check(1, mode='dedicated', requests=2)

    def test_dedicated_exact_stage_readback_and_cleanup(self):
        package = self.dedicated_offer()
        result = self.check(0, mode='dedicated-stage', requests=4, package=package)
        self.assertIn('Dedicated install job prepared:', result)
        self.assertIn('WRITER CHECK result=0', result)
        self.assertIn('STAGE result=0', result)

    def test_dedicated_bad_stage_and_cancel_both_clean_up(self):
        package = self.dedicated_offer()
        corrupted = package[:-1] + bytes([1 ^ package[-1]])
        self.assertIn('verification and job preparation: FAIL', self.check(1, mode='dedicated-stage', requests=4, package=corrupted))
        self.check(1, mode='dedicated-stage-cancel', requests=4, package=package)

    def test_dedicated_incomplete_or_changed_components_never_publish_job(self):
        package = self.dedicated_offer()
        for mode in ('dedicated-stage-short-write', 'dedicated-stage-bad-readback'):
            with self.subTest(mode=mode):
                result = self.check(1, mode=mode, requests=4, package=package)
                self.assertIn('verification and job preparation: FAIL', result)
                self.assertNotIn('Dedicated install job prepared:', result)

    def test_dedicated_writer_rejects_change_after_download(self):
        package = self.dedicated_offer()
        result = self.check(1, mode='dedicated-stage-altered-handoff', requests=4, package=package)
        self.assertIn('Dedicated install job prepared:', result)
        self.assertIn('WRITER CHECK result=-1', result)

    def test_dedicated_gates_power_and_storage_before_requests(self):
        package = self.dedicated_offer()
        self.assertIn('STAGE result=-3', self.check(1, mode='dedicated-stage-power', requests=2, package=package))
        self.assertIn('STAGE result=-2', self.check(1, mode='dedicated-stage-space', requests=2, package=package))
