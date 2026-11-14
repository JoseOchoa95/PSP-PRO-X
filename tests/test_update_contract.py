"""Security cases for production release selection, contract and safe ZIP staging.

Run with PROX_RUN_CONTRACT_TESTS=1 inside the pinned offline Docker image after
the native TLS dependencies have been built. Keys/fixtures remain in artifacts.
"""
from pathlib import Path
import copy
import hashlib
import io
import json
import os
import struct
import subprocess
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT/'artifacts/phase3/contract-native'
NAMES = ['PROUPDATE/EBOOT.PBP', 'FastRecovery/EBOOT.PBP', 'PROXUPDATE/EBOOT.PBP',
         'PROXUPDATE/cacert.pem', 'LICENSE', 'THIRD_PARTY_NOTICES.md', 'SOURCE.md', 'licenses/mbedtls/LICENSE']
PREFIX = 'https://github.com/JoseOchoa95/PSP-PRO-X/releases/download/v0.4.0/'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def archive(files, transform=None):
    buffer = io.BytesIO()
    with zipfile.ZipFile(buffer, 'w') as z:
        for name, data in files.items():
            entry = zipfile.ZipInfo(name)
            entry.external_attr = 0o100644 << 16
            entry.compress_type = zipfile.ZIP_STORED
            if transform:
                transform(entry)
            z.writestr(entry, data)
    return buffer.getvalue()

def fixture():
    value=b'PRO-X fixture\0'
    sfo=b'\0PSF'+struct.pack('<4I',0x101,36,44,1)+struct.pack('<HHIII',0,0x204,len(value),len(value),0)+b'TITLE\0\0\0'+value
    psp=bytearray(0x150); psp[:4]=b'~PSP'; struct.pack_into('<I',psp,0x2c,len(psp))
    end=40+len(sfo)
    pbp=b'\0PBP'+struct.pack('<9I',0x10000,40,*([end]*6),end+len(psp))+sfo+psp
    files = {n: pbp if n.endswith('EBOOT.PBP') else b'test only\n' for n in NAMES}
    package = archive(files)
    m = dict(schema_version=1, product='PRO-X', repository='JoseOchoa95/PSP-PRO-X',
             version='0.4.0', tag='v0.4.0', channel='stable', firmware_target='6.61',
             supported_models=[1], minimum_updater_version='0.3.0', build_id='pro-x-123456789abc',
             summary='Isolated fixture; never a published release.',
             package_asset=dict(name='pro-x-0.4.0-661-update.zip', size=len(package), sha256=sha(package)),
             files=[dict(path=n, size=len(d), sha256=sha(d)) for n, d in files.items()])
    r = dict(tag_name='v0.4.0', name='Stable ordinary name', draft=False, prerelease=False,
             assets=[dict(name='pro-x-update.json', size=1000,
                          browser_download_url=PREFIX+'pro-x-update.json'),
                     dict(name='pro-x-0.4.0-661-update.zip', size=len(package),
                          browser_download_url=PREFIX+'pro-x-0.4.0-661-update.zip', digest='sha256:'+sha(package)),
                     dict(name='pro-x-0.4.0-661-source.zip', size=500, browser_download_url=PREFIX+'source.zip')])
    return r, m, files, package

@unittest.skipUnless(os.environ.get('PROX_RUN_CONTRACT_TESTS') == '1', 'requires isolated native contract environment')
class ContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        WORK.mkdir(parents=True, exist_ok=True)
        tls = ROOT/'artifacts/phase3/https-native/mbedtls'
        if not (tls/'library/libmbedcrypto.a').exists():
            from test_update_https import prepare
            prepare()
        command = ['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-DPROX_OTA_NATIVE',
                   '-DMBEDTLS_CONFIG_FILE="prox_mbedtls_config.h"', '-I'+str(ROOT/'firmware/Updater'),
                   '-I'+str(tls/'include'), str(ROOT/'tests/native/test_update_contract.c')]
        command += [str(ROOT/'firmware/Updater'/n) for n in ('json.c', 'contract.c', 'policy.c', 'stage.c')]
        command += ['-L'+str(tls/'library'), '-lmbedcrypto', '-lz', '-o', str(WORK/'contract-test')]
        subprocess.run(command, check=True, capture_output=True)
        flow = list(command)
        flow[flow.index(str(ROOT/'tests/native/test_update_contract.c'))] = str(ROOT/'tests/native/test_update_flow.c')
        flow.insert(flow.index('-L'+str(tls/'library')), str(ROOT/'firmware/Updater/update.c'))
        flow.insert(1,'-I'+str(ROOT/'firmware/Common'))
        flow[-1] = str(WORK/'flow-test')
        subprocess.run(flow, check=True, capture_output=True)
        include=WORK/'target-fixture'; include.mkdir(exist_ok=True)
        (include/'prox_product.h').write_text('#define PROX_PRODUCT_VERSION "0.4.0"\n')
        installer=list(command)
        installer[installer.index(str(ROOT/'tests/native/test_update_contract.c'))]=str(ROOT/'tests/native/test_ota_installer.c')
        installer.insert(installer.index('-L'+str(tls/'library')),str(ROOT/'firmware/Updater/ota_installer.c'))
        installer[1:1]=['-DPROX_OTA_FAULT_TEST','-I'+str(include),'-I'+str(ROOT/'tests/native'),'-I'+str(ROOT/'firmware/Common')]
        installer[-1]=str(WORK/'installer-test')
        subprocess.run(installer, check=True, capture_output=True)

    def setUp(self):
        self.r, self.m, self.files, self.package = fixture()

    def check(self, expected, package=None, cancel=None, release_text=None, manifest_text=None):
        with tempfile.TemporaryDirectory(dir=WORK) as directory:
            d = Path(directory)
            (d/'release.json').write_text(release_text if release_text is not None else json.dumps(self.r))
            (d/'manifest.json').write_text(manifest_text if manifest_text is not None else json.dumps(self.m))
            cmd = [str(WORK/'contract-test'), 'contract', str(d/'release.json'), str(d/'manifest.json')]
            if package is not None:
                (d/'package.zip').write_bytes(package)
                (d/'stage').mkdir()
                cmd[1] = 'stage'; cmd += [str(d/'package.zip'), str(d/'stage')]
                if cancel is not None:
                    cmd.append(str(cancel))
            r = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
            self.assertEqual(r.returncode, expected, r.stdout+r.stderr)
            if expected == 0 and package is not None:
                self.assertEqual({p.relative_to(d/'stage').as_posix(): p.read_bytes()
                                  for p in (d/'stage').rglob('*') if p.is_file()}, self.files)
            # Native runner has no installation/flash interface. Exact paths in
            # a fresh root are checked; traversal must never create this sibling.
            self.assertFalse((d/'outside').exists())

    def bind(self, package):
        self.m['package_asset'].update(size=len(package), sha256=sha(package))
        self.r['assets'][1].update(size=len(package), digest='sha256:'+sha(package))

    def json_case(self, raw, expected):
        with tempfile.TemporaryDirectory(dir=WORK) as directory:
            p=Path(directory)/'input.json'; p.write_bytes(raw)
            r=subprocess.run([str(WORK/'contract-test'),'json',str(p)],capture_output=True,timeout=15)
            self.assertEqual(r.returncode,expected,r.stdout+r.stderr)

    def test_valid_contract_and_stored_zip(self): self.check(0, self.package)
    def test_draft_without_beta_name(self): self.r['draft'] = True; self.check(2)
    def test_prerelease_without_beta_name(self): self.r['prerelease'] = True; self.check(2)
    def test_missing_flags(self): del self.r['prerelease']; self.check(1)
    def test_old_equal_version_not_offered(self):
        for tag in ('v0.2.0', 'v0.3.0'):
            self.r['tag_name'] = tag; self.check(2)
    def test_prerelease_version_not_offered(self): self.r['tag_name'] += '-rc.1'; self.check(2)
    def test_foreign_repository_asset(self): self.r['assets'][1]['browser_download_url'] = PREFIX.replace('PSP-PRO-X', 'OTHER')+'pro-x-0.4.0-661-update.zip'; self.check(1)
    def test_http_asset(self): self.r['assets'][1]['browser_download_url'] = self.r['assets'][1]['browser_download_url'].replace('https:', 'http:'); self.check(1)
    def test_source_diagnostic_only(self): self.r['assets'] = self.r['assets'][2:]; self.check(2)
    def test_duplicate_matching_assets(self): self.r['assets'].append(copy.deepcopy(self.r['assets'][1])); self.check(1)
    def test_manifest_version_mismatch(self): self.m['version'] = '0.5.0'; self.check(1)
    def test_unsupported_schema(self): self.m['schema_version'] = 2; self.check(1)
    def test_wrong_model(self): self.m['supported_models'] = [0]; self.check(1)
    def test_newer_minimum_client(self): self.m['minimum_updater_version'] = '0.4.0'; self.check(1)
    def test_wrong_base_firmware(self): self.m['firmware_target'] = '6.60'; self.check(1)
    def test_wrong_channel(self): self.m['channel'] = 'beta'; self.check(1)
    def test_bad_build_identity(self): self.m['build_id'] = '../../outside'; self.check(1)
    def test_github_digest_conflict(self): self.r['assets'][1]['digest'] = 'sha256:'+'0'*64; self.check(1)
    def test_missing_required_file(self): self.m['files'].pop(); self.check(1)
    def test_duplicate_manifest_path(self): self.m['files'][0] = self.m['files'][1]; self.check(1)
    def test_manifest_path_traversal(self): self.m['files'][0]['path'] = '../outside'; self.check(1)
    def test_invalid_json(self): self.check(1, manifest_text='{"schema_version":1,}')
    def test_duplicate_json_property(self): self.check(1, manifest_text=json.dumps(self.m).replace('"schema_version": 1', '"schema_version": 1, "schema_version": 1'))
    def test_json_unicode_notes(self): self.json_case('{"body":"Español 😀","escaped":"\\ud83d\\ude00"}'.encode(),0)
    def test_json_raw_nul(self): self.json_case(b'{}\x00',1)
    def test_json_bad_utf8(self): self.json_case(b'{"body":"\xff"}',1)
    def test_json_utf8_surrogate(self): self.json_case(b'{"body":"\xed\xa0\x80"}',1)
    def test_json_unpaired_surrogate(self): self.json_case(b'{"body":"\\ud800"}',1)
    def test_json_depth_limit(self): self.json_case(b'['*18+b'0'+b']'*18,1)
    def test_json_token_limit(self): self.json_case(json.dumps([0]*8200).encode(),1)
    def test_json_bad_numbers(self):
        for raw in (b'[01]',b'[1.]',b'[1e]',b'[-]',b'[+1]'): self.json_case(raw,1)
    def test_package_hash_mismatch(self): self.check(1, self.package[:-1]+b'X')
    def test_cancel_before_extract(self): self.check(1, self.package, cancel=1)
    def test_truncated_zip(self): p = self.package[:-10]; self.bind(p); self.check(1, p)
    def test_zip_traversal(self):
        files = dict(self.files); files['../outside'] = files.pop(NAMES[0]); p=archive(files); self.bind(p); self.check(1, p)
    def test_zip_absolute_path(self):
        files = dict(self.files); files['/outside'] = files.pop(NAMES[0]); p=archive(files); self.bind(p); self.check(1, p)
    def test_zip_symlink(self):
        p=archive(self.files, lambda e: setattr(e, 'external_attr', 0o120777<<16)); self.bind(p); self.check(1,p)
    def test_zip_compression_rejected(self):
        p=archive(self.files, lambda e: setattr(e, 'compress_type', zipfile.ZIP_DEFLATED)); self.bind(p); self.check(1,p)
    def test_zip_extra_field_rejected(self):
        p=archive(self.files, lambda e: setattr(e, 'extra', b'\x01\x00\x00\x00')); self.bind(p); self.check(1,p)
    def test_file_hash_mismatch(self): self.m['files'][0]['sha256'] = '0'*64; self.check(1, self.package)
    def test_pbp_offsets_rejected(self):
        self.files[NAMES[0]]=b'not a PBP header; more than forty bytes!!!'+b'x'*8
        self.m['files'][0].update(size=len(self.files[NAMES[0]]), sha256=sha(self.files[NAMES[0]]))
        p=archive(self.files); self.bind(p); self.check(1,p)
    def modified_pbp(self, mutate, expected=1):
        data=bytearray(self.files[NAMES[0]]); mutate(data)
        self.files[NAMES[0]]=bytes(data)
        self.m['files'][0].update(size=len(data),sha256=sha(data))
        p=archive(self.files); self.bind(p); self.check(expected,p)
    def test_pbp_sfo_header_rejected(self): self.modified_pbp(lambda b: b.__setitem__(40,0x41))
    def test_pbp_sfo_value_bounds_rejected(self): self.modified_pbp(lambda b: struct.pack_into('<I',b,40+36-4,0xffffffff))
    def test_pbp_psp_payload_size_rejected(self):
        self.modified_pbp(lambda b: struct.pack_into('<I',b,struct.unpack_from('<I',b,32)[0]+0x2c,1))
    def test_pbp_legacy_title_length_excludes_nul(self):
        self.modified_pbp(lambda b: struct.pack_into('<I',b,40+20+4,len(b'PRO-X fixture')),expected=0)
    def test_pbp_unterminated_title_rejected(self):
        self.modified_pbp(lambda b: b.__setitem__(40+44+len(b'PRO-X fixture'),ord('X')))

    def flow(self, mode='ok', expected=0, catalog=None, manifest_raw=None):
        with tempfile.TemporaryDirectory(dir=WORK) as directory:
            d=Path(directory); (d/'client').mkdir()
            manifest=json.dumps(self.m).encode() if manifest_raw is None else manifest_raw
            self.r['assets'][0]['size']=len(manifest)
            (d/'catalog.json').write_text(json.dumps([self.r] if catalog is None else catalog))
            (d/'manifest.json').write_bytes(manifest); (d/'package.zip').write_bytes(self.package)
            r=subprocess.run([str(WORK/'flow-test'),str(d),str(d/'client'),mode],capture_output=True,text=True,timeout=15)
            self.assertEqual(r.returncode,expected,r.stdout+r.stderr)
            stage=d/'client/stage'
            if expected == 0:
                self.assertIn('FLOW result=1',r.stdout)
                self.assertEqual({n:(stage/n).read_bytes() for n in self.files},self.files)
                self.assertFalse((stage/'update.part').exists())
            else:
                self.assertFalse(stage.exists(),r.stdout)
            self.assertFalse((d/'client/EBOOT.PBP').exists())
            return r.stdout

    def test_flow_ready_only_after_two_confirmations(self): self.assertIn('confirmations=2',self.flow())
    def test_flow_no_space_no_stage(self): self.flow('no-space',1)
    def test_flow_bad_power_removes_stage(self): self.flow('no-power',1)
    def test_flow_cancel_before_download(self): self.flow('cancel-download',1)
    def test_flow_cancel_before_install(self): self.flow('cancel-install',1)
    def test_flow_changed_live_version(self): self.flow('changed-state',1)
    def test_flow_bad_hash_removes_stage(self): self.flow('bad-package-hash',1)
    def test_flow_api_404_not_current(self): self.assertIn('no up-to-date decision',self.flow('api-error',1))
    def test_flow_rate_limit_no_retry(self): self.assertIn('requests=1',self.flow('rate-limit',1))
    def test_flow_network_error_not_current(self): self.assertNotIn('No newer compatible',self.flow('network-error',1))
    def test_flow_empty_catalog(self): self.flow(expected=2,catalog=[])
    def test_flow_draft_excluded(self): self.r['draft']=True; self.flow(expected=2)
    def test_flow_prerelease_excluded(self): self.r['prerelease']=True; self.flow(expected=2)
    def test_flow_duplicate_version_rejected(self): self.flow(expected=1,catalog=[self.r,self.r])
    def test_flow_manifest_json_error_not_current(self): self.assertIn('no update decision',self.flow(expected=1,manifest_raw=b'{broken'))
    def test_flow_manifest_digest_error_not_current(self):
        self.r['assets'][0]['digest']='sha256:'+'0'*64
        self.assertIn('no update decision',self.flow(expected=1))
    def test_flow_incompatible_contract_not_current(self):
        self.m['supported_models']=[0]
        self.assertIn('not an up-to-date result',self.flow(expected=2))

    def installer(self, mode='ok', expected=0, action='commit', source='0.3.0', bad_hash=False, stale=False):
        with tempfile.TemporaryDirectory(dir=WORK) as directory:
            d=Path(directory); game=d/'ms0:/PSP/GAME'
            stage=game/'PROXUPDATE/.stage-pro-x-123456789abc-00000001'
            stage.mkdir(parents=True)
            old={}
            for i,(name,data) in enumerate(self.files.items()):
                (stage/name).parent.mkdir(parents=True,exist_ok=True); (stage/name).write_bytes(data)
                target=game/name if i < 4 else game/'PROXUPDATE'/name
                target.parent.mkdir(parents=True,exist_ok=True)
                target.write_bytes(b'original-'+name.encode()); old[target]=target.read_bytes()
            if stale:
                (game/'FastRecovery/EBOOT.PBP.prox-old').write_bytes(b'preserved backup')
            raw=json.dumps(self.m).encode(); (stage/'pro-x-update.json').write_bytes(raw)
            path='ms0:/PSP/GAME/PROXUPDATE/'+stage.name+'/PROUPDATE/installer.prx'
            digest='0'*64 if bad_hash else sha(raw)
            r=subprocess.run([str(WORK/'installer-test'),path,digest,source,mode,action],cwd=d,
                             capture_output=True,text=True,timeout=15)
            self.assertEqual(r.returncode,expected,r.stdout+r.stderr)
            self.assertIn('NO FLASH INSTALLATION CAPABILITY',r.stdout)
            if expected != 0 or action != 'commit':
                for target,data in old.items(): self.assertEqual(target.read_bytes(),data,target)
            else:
                for i,(name,data) in enumerate(self.files.items()):
                    target=game/name if i < 4 else game/'PROXUPDATE'/name
                    self.assertEqual(target.read_bytes(),data,target)
                self.assertFalse(list(game.rglob('*.prox-old')))
            if mode in ('cancel-preflight','cancel-after-prepare','space-short','power-lost','lock-failure') or stale:
                self.assertFalse(list(game.rglob('*.prox-new')))
            if stale: self.assertEqual((game/'FastRecovery/EBOOT.PBP.prox-old').read_bytes(),b'preserved backup')
            return r.stdout

    def test_installer_begin_rechecks_manifest_and_files(self): self.installer(action='begin')
    def test_installer_storage_commit_fixture(self): self.installer()
    def test_installer_manifest_hash_rejected(self): self.installer(expected=1,bad_hash=True)
    def test_installer_equal_source_rejected(self): self.installer(expected=1,source='0.4.0')
    def test_installer_newer_source_rejected(self): self.installer(expected=1,source='0.5.0')
    def test_installer_cancel_before_write(self): self.installer('cancel-preflight',1)
    def test_installer_cancel_after_prepare_cleans_temporaries(self): self.installer('cancel-after-prepare',1)
    def test_installer_space_short_before_write(self): self.installer('space-short',1)
    def test_installer_power_lost_before_write(self): self.installer('power-lost',1)
    def test_installer_lock_failure_cleans_temporaries(self): self.installer('lock-failure',1)
    def test_installer_stale_backup_preserved(self): self.installer(expected=1,stale=True)
    def test_installer_rename_failure_restores_storage(self): self.installer('rename-failure',1)

if __name__ == '__main__': unittest.main(verbosity=2)
