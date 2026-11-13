from pathlib import Path
import gzip
import hashlib
import importlib.util
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / 'scripts'))
from verify import pbp_info


class PackagingTests(unittest.TestCase):
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
