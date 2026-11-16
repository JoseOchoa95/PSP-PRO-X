from pathlib import Path
import gzip
import hashlib
import importlib.util
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
from native_release import require_release_variant
from native_validation_report import for_build, TESTED_FINAL
import copy


class PackagingTests(unittest.TestCase):
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
        for flag in ('native_query_only','native_offer_preview','native_progress_preview','test_only','simulated_positive_offer','simulated_incomplete_transfer'):
            with self.assertRaises(RuntimeError): require_release_variant({**valid,flag:True})
        with self.assertRaises(RuntimeError): require_release_variant({'mode':'pro-x','native_ota':False})
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
