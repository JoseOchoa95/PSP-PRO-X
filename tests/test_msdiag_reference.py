import importlib.util
from pathlib import Path
import unittest


source = Path(__file__).resolve().parents[1] / "firmware/tools/msdiag/capture-reference.py"
spec = importlib.util.spec_from_file_location("capture_reference", source)
capture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture)


class ReferenceTest(unittest.TestCase):
    def test_distinct_samples_at_both_cutoffs_and_near_end(self):
        for size in (256_000_000_000, 1_000_000_000_000):
            with self.subTest(size=size):
                sectors = size // capture.SECTOR_BYTES
                dec, binary = capture.CUTOFFS
                populated = {0, capture.WINDOW, dec - 2, dec + 1, binary - 2,
                             binary + 1, sectors - 3}

                def read(lba):
                    self.assertIn(lba, range(sectors))
                    return (lba.to_bytes(8, "little") if lba in populated else b"").ljust(512, b"\0")

                data = capture.create_reference(read, size, 512)
                self.assertEqual(capture.HEADER.unpack_from(data), (b"MSDREF01", size, 512, 0))
                self.assertEqual(len(data), 248)
                lbas = [r[0] for r in capture.RECORD.iter_unpack(data[capture.HEADER.size:])]
                self.assertEqual(lbas, [0, capture.WINDOW, dec - 2, dec + 1, binary - 2,
                                        binary + 1, sectors - 3, sectors - 1])

    def test_smaller_card_skips_outside_probes(self):
        size = 128_000_000_000
        sectors = size // 512

        def read(lba):
            self.assertIn(lba, range(sectors))
            return bytes(512)

        data = capture.create_reference(read, size, 512)
        lbas = [r[0] for r in capture.RECORD.iter_unpack(data[capture.HEADER.size:])]
        self.assertEqual(lbas[2:6], [sectors - 1, *[capture.MISSING_LBA] * 3])

    def test_matching_wrap_reference_selects_distinct_neighbor(self):
        boundary = capture.CUTOFFS[0]

        def read(lba):
            markers = {0: b"A", boundary: b"A", boundary + 1: b"B"}
            return markers.get(lba, b"").ljust(512, b"\0")

        lba, _ = capture.choose_sample(read, boundary, 1, boundary + 20)
        self.assertEqual(lba, boundary + 1)

    def test_invalid_geometry_and_failed_read(self):
        for size, sector_size in ((256_000_000_000, 4096), (256_000_000_001, 512), (512, 512)):
            with self.subTest(size=size, sector_size=sector_size):
                with self.assertRaises(ValueError):
                    capture.create_reference(lambda _: b"", size, sector_size)
        with self.assertRaisesRegex(OSError, "Short read at LBA 0"):
            capture.create_reference(lambda _: b"", 256_000_000_000, 512)


if __name__ == "__main__":
    unittest.main()
