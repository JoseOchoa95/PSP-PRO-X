from pathlib import Path
import gzip
import hashlib
import importlib.util
import json
import struct
import sys
import tempfile
import unittest
from argparse import Namespace

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / 'scripts'))
from verify import pbp_info
from build import configure_native_mode
from native_release import read_checksums, require_release_variant, resident_upload_paths, verify_resident_uploads
from native_validation_report import for_build, TESTED_FINAL, BACKGROUND_FINAL
import copy


class PackagingTests(unittest.TestCase):
    def test_background_acceptance_does_not_transfer_to_changed_bytes(self):
        build=copy.deepcopy(BACKGROUND_FINAL)
        report=for_build(build)
        self.assertEqual(report['final_build']['system_software_label'],'PASS user-reported: 6.61 PRO-X 0.3.4')
        report['final_build']['select_menu']='changed outside the report'
        self.assertEqual(for_build(build)['final_build']['select_menu'],'PASS user-reported')
        for key in ('build_id','product_version','source_tree_sha256'):
            changed=copy.deepcopy(build);changed[key]='different'
            self.assertIsNone(for_build(changed))
        for name in build['artifacts']:
            changed=copy.deepcopy(build);changed['artifacts'][name]='0'*64
            self.assertIsNone(for_build(changed))
    def test_hardware_acceptance_is_bound_to_exact_delivered_bytes(self):
        build=copy.deepcopy(TESTED_FINAL)
        self.assertEqual(for_build(build)['status'],'USER_REPORTED_PARTIAL_PASS')
        for key in ('build_id','product_version','source_tree_sha256'):
            other=copy.deepcopy(build); other[key]='different'
            self.assertIsNone(for_build(other))
        other=copy.deepcopy(build); other['artifacts']['GAME/PROUPDATE/EBOOT.PBP']='0'*64
        self.assertIsNone(for_build(other))
    def test_production_default_matches_explicit_native_build(self):
        default=Namespace(mode='pro-x',native_query=False,native_offer_preview=False,native_progress_preview=False,native_ota=False)
        explicit=Namespace(**vars(default)); explicit.native_ota=True
        configure_native_mode(default); configure_native_mode(explicit)
        self.assertEqual(vars(default),vars(explicit)); self.assertTrue(default.native_ota)
        self.assertTrue(default.native_background)

    def test_reference_and_explicit_diagnostic_modes_stay_separate(self):
        for mode,flag in [('reference',None),('pro-x','native_query'),('pro-x','native_offer_preview'),('pro-x','native_progress_preview')]:
            args=Namespace(mode=mode,native_query=False,native_offer_preview=False,native_progress_preview=False,native_ota=False)
            if flag: setattr(args,flag,True)
            configure_native_mode(args); self.assertFalse(args.native_ota)
        args=Namespace(mode='reference',native_query=True,native_offer_preview=False,native_progress_preview=False,native_ota=False)
        with self.assertRaises(ValueError): configure_native_mode(args)

    def test_only_real_non_temporary_variant_can_generate_release_assets(self):
        valid={'mode':'pro-x','native_ota':True}
        require_release_variant(valid)
        for flag in ('native_query_only','native_offer_preview','native_progress_preview','native_install_preview','test_only','simulated_positive_offer','simulated_incomplete_transfer'):
            with self.assertRaises(RuntimeError): require_release_variant({**valid,flag:True})
        with self.assertRaises(RuntimeError): require_release_variant({'mode':'pro-x','native_ota':False})

    def test_resident_versions_require_the_writable_dedicated_build(self):
        from product import VERSION
        legacy={'mode':'pro-x','native_ota':True,'native_background':True,'product_version':'0.3.4'}
        require_release_variant(legacy)
        release=dict(legacy,product_version=VERSION,resident_query=True,resident_dedicated_ota=True)
        require_release_variant(release)
        for flag in ('resident_query','resident_dedicated_ota','native_background'):
            with self.subTest(flag=flag), self.assertRaises(RuntimeError):
                require_release_variant({**release,flag:False})
        with self.assertRaises(RuntimeError):
            require_release_variant(dict(legacy,product_version=VERSION))
        with self.assertRaises(RuntimeError):
            require_release_variant(dict(release,resident_dedicated_preview=True))

    def test_resident_distribution_checks_the_exact_dedicated_pair(self):
        from dedicated_update import PARTS, release_manifest, render_parts
        from product import VERSION

        spec=importlib.util.spec_from_file_location('verify_package',ROOT/'scripts/verify-package.py')
        verify_package=importlib.util.module_from_spec(spec)
        spec.loader.exec_module(verify_package)
        image,identity=render_parts((bytes([i])*52 for i in range(len(PARTS))),
                                    'pro-x-123456789abc',VERSION)
        manifest=dict(version=VERSION,build_id=identity['build_id'],ota_contract=identity['format'],
                      dedicated_update=identity,embedded_modules={PARTS[-1]:identity['parts'][-1]['sha256']})
        with tempfile.TemporaryDirectory() as name:
            directory=Path(name)
            manual=directory/'manual.zip'
            package=directory/'EBOOT.PBP'
            metadata=directory/'pro-x-dedicated-update.json'
            package.write_bytes(image)
            contract=release_manifest(identity)
            metadata.write_text(json.dumps(contract))
            self.assertEqual(verify_package.verify_resident_assets(manual,manifest),(package,metadata))
            metadata.write_text(json.dumps(dict(contract,minimum_updater_version='0.3.4')))
            with self.assertRaises(RuntimeError):
                verify_package.verify_resident_assets(manual,manifest)
            metadata.write_text(json.dumps(contract))
            package.write_bytes(image[:-1]+bytes([image[-1]^1]))
            with self.assertRaises(RuntimeError):
                verify_package.verify_resident_assets(manual,manifest)
            package.write_bytes(image)
            (directory/'pro-x-update.json').write_text('{}')
            with self.assertRaises(RuntimeError):
                verify_package.verify_resident_assets(manual,manifest)

    def test_resident_uploads_accept_only_six_files_and_hash_the_other_five(self):
        with tempfile.TemporaryDirectory() as name:
            directory = Path(name)
            package = directory / 'pro-x-0.4.3-661.zip'
            uploads = resident_upload_paths(package)
            self.assertEqual({path.name for path in uploads}, {
                'pro-x-0.4.3-661.zip', 'pro-x-0.4.3-661-source.zip', 'pro-x-0.4.3-661-build.json',
                'pro-x-dedicated-update.json', 'EBOOT.PBP', 'pro-x-0.4.3-661.sha256',
            })
            (directory / 'release').mkdir()
            for path in uploads[:-1]:
                path.write_bytes(path.name.encode())
            checksums = {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in uploads[:-1]}
            checksum_list = uploads[-1]
            checksum_list.write_text(''.join(f'{value}  {name}\n' for name, value in checksums.items()), encoding='ascii')
            self.assertEqual(read_checksums(checksum_list), checksums)
            uploaded = verify_resident_uploads(package, checksums)
            self.assertEqual([entry['name'] for entry in uploaded], [path.name for path in uploads])
            self.assertEqual(uploaded[-1]['sha256'], hashlib.sha256(checksum_list.read_bytes()).hexdigest())

            extra = directory / 'pro-x-0.4.3-661-manifest.json'
            extra.write_text('{}')
            with self.assertRaisesRegex(RuntimeError, 'Unexpected item'):
                verify_resident_uploads(package, checksums)
            extra.unlink()
            with self.assertRaisesRegex(RuntimeError, 'checksum inventory'):
                verify_resident_uploads(package, {**checksums, checksum_list.name: '0' * 64})
            with self.assertRaisesRegex(RuntimeError, 'checksum inventory'):
                verify_resident_uploads(package, {path: value for path, value in checksums.items() if path != 'EBOOT.PBP'})
            payload = directory / 'EBOOT.PBP'
            payload.write_bytes(b'changed')
            with self.assertRaisesRegex(RuntimeError, 'upload hash mismatch'):
                verify_resident_uploads(package, checksums)
            payload.unlink()
            with self.assertRaisesRegex(RuntimeError, 'Missing release assets: EBOOT.PBP'):
                verify_resident_uploads(package, checksums)

    def test_external_checksums_reject_duplicate_and_traversing_paths(self):
        with tempfile.TemporaryDirectory() as name:
            checksum_list = Path(name) / 'release.sha256'
            row = '0' * 64 + '  EBOOT.PBP\n'
            checksum_list.write_text(row + row, encoding='ascii')
            with self.assertRaisesRegex(RuntimeError, 'duplicate checksum entry'):
                read_checksums(checksum_list)
            checksum_list.write_text('0' * 64 + '  ../EBOOT.PBP\n', encoding='ascii')
            with self.assertRaisesRegex(RuntimeError, 'Invalid or duplicate checksum entry'):
                read_checksums(checksum_list)

    def test_background_preview_is_explicit_and_excluded_from_stable_assets(self):
        args=Namespace(mode='pro-x',native_query=False,native_offer_preview=False,native_progress_preview=False,native_ota=False,native_install_preview=True)
        configure_native_mode(args)
        self.assertTrue(args.native_query and args.native_ota and args.native_install_preview)
        self.assertFalse(args.native_background)
        with self.assertRaises(RuntimeError): require_release_variant({'mode':'pro-x','native_ota':True,'native_install_preview':True})
        args=Namespace(mode='pro-x',native_query=False,native_offer_preview=True,native_progress_preview=False,native_ota=False,native_install_preview=True)
        with self.assertRaises(ValueError): configure_native_mode(args)
    def test_python3_prx_header_roundtrip(self):
        spec = importlib.util.spec_from_file_location('pspgz', ROOT / 'firmware/contrib/pspgz.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            payload = b'\x7fELF' + bytes(range(256)) * 4
            (root / 'input').write_bytes(payload)
            output = root / 'output'
            self.assertEqual(module.prx_compress(output, ROOT / 'firmware/contrib/SystemControl.hdr', root / 'input', 'TestModule', 0x1007), 0)
            data = output.read_bytes()
            self.assertEqual(data[:4], b'~PSP')
            self.assertEqual(data[10:38].rstrip(b'\0'), b'TestModule')
            self.assertEqual(struct.unpack_from('<H', data, 4)[0], 0x1007)
            self.assertEqual(struct.unpack_from('<I', data, 0x28)[0], len(payload))
            self.assertEqual(struct.unpack_from('<I', data, 0x2c)[0], len(data))
            self.assertEqual(struct.unpack_from('<I', data, 0xb0)[0], len(data) - 0x150)
            self.assertEqual(data[0x140:0x150], hashlib.md5(data[0x150:]).digest())
            self.assertEqual(gzip.decompress(data[0x150:]), payload)
            self.assertEqual(struct.unpack_from('<I', data, 0x154)[0], 1225856967)
            module.prx_compress(root / 'second', ROOT / 'firmware/contrib/SystemControl.hdr', root / 'input', 'TestModule', 0x1007)
            self.assertEqual((root / 'second').read_bytes(), data)

    def test_pbp_rejects_invalid_offsets(self):
        bad = b'\0PBP' + struct.pack('<9I', 0x10000, 40, 39, 40, 40, 40, 40, 40, 40)
        with self.assertRaisesRegex(RuntimeError, 'offsets'):
            pbp_info(bad)

    def test_pbp_rejects_truncated_header(self):
        with self.assertRaisesRegex(RuntimeError, 'header'):
            pbp_info(b'\0PBP')

    def test_pbp_rejects_wrong_magic(self):
        with self.assertRaisesRegex(RuntimeError, 'magic'):
            pbp_info(b'FAKE' + bytes(36))


if __name__ == '__main__':
    unittest.main()
