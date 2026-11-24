"""Consent-bound native download/staging; fixture transport, no PSP flash."""
from pathlib import Path
import hashlib
import json
import os
import subprocess
import tempfile
import unittest
from test_update_contract import LEGACY_CLIENT_VERSION, archive, fixture, fixture_product_header

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'artifacts/phase3/native-download'


@unittest.skipUnless(os.environ.get('PROX_RUN_CONTRACT_TESTS')=='1', 'requires offline native environment')
class NativeDownloadTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        WORK.mkdir(parents=True,exist_ok=True)
        tls=ROOT/'artifacts/phase3/https-native/mbedtls'
        if not (tls/'library/libmbedcrypto.a').exists():
            from test_update_https import prepare
            prepare()
        client = fixture_product_header(WORK/'client-fixture', LEGACY_CLIENT_VERSION)
        cmd=['cc','-std=c99','-Wall','-Wextra','-Werror','-DPROX_OTA_NATIVE',
             '-DMBEDTLS_CONFIG_FILE="prox_mbedtls_config.h"',
             '-I'+str(client),
             '-I'+str(ROOT/'firmware/Updater'),'-I'+str(ROOT/'firmware/Common'),'-I'+str(tls/'include'),
             str(ROOT/'tests/native/test_native_download.c')]
        cmd += [str(ROOT/'firmware/Updater'/n) for n in ('native_download.c','json.c','contract.c','policy.c','stage.c')]
        cmd += ['-L'+str(tls/'library'),'-lmbedcrypto','-lz','-o',str(WORK/'download-test')]
        result=subprocess.run(cmd,capture_output=True,text=True)
        if result.returncode:
            raise RuntimeError(result.stderr)

    def check(self,mode='normal',success=False,requests=None,unsafe=False):
        release,manifest,files,package=fixture()
        if unsafe:
            files['../escape']=b'forbidden'
            package=archive(files)
            manifest['package_asset']['size']=len(package)
            manifest['package_asset']['sha256']=hashlib.sha256(package).hexdigest()
            release['assets'][1]['size']=len(package)
            release['assets'][1]['digest']='sha256:'+hashlib.sha256(package).hexdigest()
        raw=json.dumps(manifest).encode()
        release['assets'][0]['size']=len(raw)
        with tempfile.TemporaryDirectory(dir=WORK) as name:
            root=Path(name); destination=root/'destination'; destination.mkdir()
            (root/'catalog.json').write_text(json.dumps([release]))
            (root/'manifest.json').write_bytes(raw)
            (root/'package.zip').write_bytes(package)
            result=subprocess.run([str(WORK/'download-test'),name,str(destination),mode],capture_output=True,text=True)
            self.assertEqual(result.returncode,0 if success else 1,result.stdout+result.stderr)
            if requests is not None:
                self.assertIn('requests='+str(requests)+' ',result.stdout)
            if success:
                stage=destination/'stage'
                self.assertTrue((stage/'PROUPDATE/EBOOT.PBP').is_file())
                self.assertEqual((stage/'pro-x-update.json').read_bytes(),raw)
                self.assertFalse((stage/'update.part').exists())
            else:
                self.assertEqual(list(destination.iterdir()),[],result.stdout)
                self.assertIn('root=\n',result.stdout)
            self.assertFalse((root/'escape').exists())

    def test_verified_private_stage(self): self.check(success=True,requests=2)
    def test_progress_consumer_cancellation_removes_stage(self): self.check('observer-cancel',requests=2)
    def test_missing_snapshot_hash_before_network(self): self.check('missing-hash',requests=0)
    def test_changed_snapshot(self): self.check('snapshot-changed',requests=1)
    def test_cancel_before_network(self): self.check('cancel',requests=0)
    def test_cancel_package(self): self.check('cancel-package',requests=2)
    def test_live_model_changed_before_network(self): self.check('changed-before',requests=0)
    def test_live_model_changed_after_download(self): self.check('changed-after',requests=2)
    def test_power_required_before_stage(self): self.check('no-power',requests=1)
    def test_power_lost_after_download(self): self.check('power-lost',requests=2)
    def test_storage_required_before_stage(self): self.check('no-space',requests=1)
    def test_stage_creation_failure(self): self.check('stage-failure',requests=1)
    def test_manifest_http(self): self.check('manifest-http',requests=1)
    def test_package_http(self): self.check('package-http',requests=2)
    def test_network_failure(self): self.check('network-error',requests=1)
    def test_package_hash(self): self.check('bad-package-hash',requests=2)
    def test_package_size(self): self.check('bad-package-size',requests=2)
    def test_unsafe_zip(self): self.check(unsafe=True,requests=2)
