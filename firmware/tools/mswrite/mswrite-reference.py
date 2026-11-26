"""Prepare or independently verify PSP storage trials using a Windows direct reader."""

import argparse
import hashlib
import mmap
import os
from pathlib import Path
import runpy
import secrets
import struct
import subprocess
import tempfile


SECTOR_BYTES = 512
GEOMETRY = 500695040
REPORTED = 232259584
PARTITION_END = 500692992
TARGET = 500694016
WITNESSES = (0, 2048, 268435456, TARGET % (1 << 23), TARGET % 250000000,
             TARGET % 268435456, TARGET % REPORTED)
MAGIC = b"MSWREF01"
PARTITION_MAGIC = b"MSPREF01"
PARTITION_LAYOUT = ((2048, 230686720), (230688768, 230686720), (461375488, 39319552))
PARTITION_WITNESSES = (0, 2048, 268435456, TARGET, 230688768, 461375488, GEOMETRY - 1)


def check_layout(sector_zero, boot):
    start, length = struct.unpack_from("<II", sector_zero, 454)
    if (sector_zero[510:] != b"\x55\xaa" or sector_zero[450] != 7
            or sector_zero[462:510] != bytes(48) or start != 2048
            or start + length != PARTITION_END or not PARTITION_END < TARGET < GEOMETRY):
        raise ValueError("Expected the tested single partition with an unused tail")
    if boot[3:11] != b"EXFAT   " or struct.unpack_from("<QQ", boot, 64) != (start, length):
        raise ValueError("The exFAT boot sector does not match the test layout")


def make_reference(read):
    check_layout(read(0), read(2048))
    original = read(TARGET)
    if original not in (bytes(SECTOR_BYTES), b"\xff" * SECTOR_BYTES):
        raise ValueError("The intended scratch sector contains data")
    marker = secrets.token_bytes(SECTOR_BYTES)
    while marker == original:
        marker = secrets.token_bytes(SECTOR_BYTES)
    return MAGIC + b"".join(hashlib.sha1(read(lba)).digest() for lba in WITNESSES) + original + marker


def chs(lba):
    cylinder = lba // (255 * 63)
    if cylinder > 1023:
        return b"\xfe\xff\xff"
    return bytes((lba // 63 % 255, (cylinder >> 8) << 6 | (lba % 63 + 1), cylinder & 255))


def partition_table(original):
    table = bytearray(original)
    table[446:510] = bytes(64)
    for i, (start, count) in enumerate(PARTITION_LAYOUT):
        struct.pack_into("<B3sB3sII", table, 446 + i * 16, 0x80 if i == 0 else 0,
                         chs(start), 0x0c, chs(start + count - 1), start, count)
    return bytes(table)


def make_partition_reference(read, previous_trial):
    original = read(0)
    check_layout(original, read(2048))
    if not verify_reference(read, previous_trial):
        raise ValueError("The original high-sector trial must match this card")
    samples = b"".join(hashlib.sha1(read(lba)).digest() for lba in PARTITION_WITNESSES)
    return PARTITION_MAGIC + samples + original + partition_table(original)


def verify_reference(read, data, partitions=False):
    magic, witnesses, target = ((PARTITION_MAGIC, PARTITION_WITNESSES, 0) if partitions
                                else (MAGIC, WITNESSES, TARGET))
    payload = len(magic) + len(witnesses) * 20
    if len(data) != payload + SECTOR_BYTES * 2 or not data.startswith(magic):
        raise ValueError("Invalid trial reference")
    expected_marker = data[payload + SECTOR_BYTES:]
    actual = read(target)
    matches = actual == expected_marker
    if partitions:
        original = data[payload:payload + SECTOR_BYTES]
        if expected_marker != partition_table(original):
            raise ValueError("Reference partition entries do not match the trial layout")
        if matches:
            for i in range(3):
                start, count = struct.unpack_from("<II", actual, 454 + i * 16)
                print(f"Partition {i + 1}: start {start}, sectors {count}")
    print(f"Target LBA {target}: {'MATCH' if matches else 'MISMATCH'}")
    for index, lba in enumerate(witnesses):
        if partitions and lba == target:
            continue
        expected = data[len(magic) + index * 20:len(magic) + (index + 1) * 20]
        if hashlib.sha1(read(lba)).digest() != expected:
            print(f"Witness LBA {lba}: CHANGED")
            matches = False
    return matches


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["prepare", "verify", "prepare-partitions", "verify-partitions"])
    parser.add_argument("disk", type=int, help="Direct microSD reader disk number from Get-Disk")
    parser.add_argument("reference", nargs="?", type=Path, help="Saved trial reference")
    args = parser.parse_args()
    if os.name != "nt" or args.disk < 0:
        parser.error("Use a nonnegative Windows disk number")
    if (args.action != "prepare") != bool(args.reference):
        parser.error("Specify the saved trial reference for this action")
    get_disk = runpy.run_path(str(Path(__file__).with_name("capture-reference.py")))["get_disk"]
    try:
        disk = get_disk(args.disk)
        if (disk["Size"], disk["LogicalSectorSize"]) != (GEOMETRY * SECTOR_BYTES, SECTOR_BYTES):
            raise ValueError("Use the tested 256 GB card in the direct microSD reader")
        with (open(rf"\\.\PhysicalDrive{args.disk}", "rb", buffering=0) as card,
              mmap.mmap(-1, SECTOR_BYTES) as buffer):
            def read(lba):
                card.seek(lba * SECTOR_BYTES)
                count = card.readinto(buffer)
                if count != SECTOR_BYTES:
                    raise OSError(f"Short read at LBA {lba}")
                return buffer[:SECTOR_BYTES]

            if args.action in ("prepare", "prepare-partitions"):
                directory = Path(tempfile.mkdtemp(prefix="mswrite-"))
                if args.action == "prepare-partitions":
                    snapshot = make_partition_reference(read, args.reference.read_bytes())
                    output = directory / "MSPART.BIN"
                    (directory / "ORIGINAL-MBR.BIN").write_bytes(snapshot[-2 * SECTOR_BYTES:-SECTOR_BYTES])
                else:
                    snapshot = make_reference(read)
                    output = directory / "MSWTEST.BIN"
                output.write_bytes(snapshot)
                print(f"Copy this reference beside the trial EBOOT: {output}")
                print("Keep the PC copy for verification. The direct reader only read the card.")
            elif verify_reference(read, args.reference.read_bytes(), args.action == "verify-partitions"):
                print("PASS: the direct reader found the marker and all witnesses matched.")
            else:
                raise ValueError("Independent direct-reader verification failed")
    except (KeyError, OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"Trial reference failed: {error}\n")


if __name__ == "__main__":
    main()
