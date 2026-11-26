"""Verify the format trial with the microSD in the Windows reader's direct slot."""

import argparse
import hashlib
import json
import mmap
from pathlib import Path
import struct
import subprocess


SECTOR = 512
DISK_SECTORS = 500695040
START = 461375488
LENGTH = 39319552
FAT_SECTORS = 4799
METADATA_SECTORS = 9694
REFERENCE_SHA256 = "0dbae05326b146671910ffdc6497c56112f93994d5224f62ed1855e0f6f1b015"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def verify_identity(read, reference):
    mbr = read(0)
    require(mbr == reference[-SECTOR:], "The MBR changed.")
    entries = [struct.unpack_from("<II", mbr, 454 + index * 16) for index in range(3)]
    require(entries == [(2048, 230686720), (230688768, 230686720), (START, LENGTH)],
            "The MBR partition geometry differs from the trial layout.")
    for lba, index in ((2048, 1), (268435456, 2), (500694016, 3), (230688768, 4),
                       (DISK_SECTORS - 1, 6)):
        expected = reference[8 + index * 20:28 + index * 20]
        require(hashlib.sha1(read(lba)).digest() == expected, f"The witness at LBA {lba} changed.")


def verify_boot(read, allow_mount_status=False):
    boot = read(START)
    require(boot[:11] == b"\xeb\x58\x90PROX    " and boot[510:] == b"\x55\xaa",
            "Partition 3 does not contain the trial's FAT32 boot record.")
    require(struct.unpack_from("<HBHB", boot, 11) == (512, 64, 32, 2) and
            struct.unpack_from("<III", boot, 28) == (START, LENGTH, FAT_SECTORS) and
            struct.unpack_from("<IHH", boot, 44) == (2, 1, 6) and
            struct.unpack_from("<I", boot, 67)[0] == 0x50585031,
            "Partition 3 FAT32 geometry or serial differs from the trial.")
    backup = read(START + 6)
    if allow_mount_status:
        backup = backup[:65] + boot[65:66] + backup[66:]
    require(backup == boot, "The backup boot record differs from the primary.")


def verify(read, reference):
    verify_identity(read, reference)
    verify_boot(read)
    info = bytearray(SECTOR)
    struct.pack_into("<I", info, 0, 0x41615252)
    clusters = (LENGTH - 32 - 2 * FAT_SECTORS) // 64
    struct.pack_into("<IIII", info, 484, 0x61417272, clusters - 1, 3, 0)
    struct.pack_into("<I", info, 508, 0xaa550000)
    fat = struct.pack("<III", 0x0ffffff8, 0x0fffffff, 0x0fffffff).ljust(SECTOR, b"\0")
    special = {1: info, 7: info, 32: fat, 32 + FAT_SECTORS: fat}
    zero = bytes(SECTOR)
    for relative in range(1, METADATA_SECTORS):
        if relative != 6:
            require(read(START + relative) == special.get(relative, zero),
                    f"Unexpected FAT32 metadata at relative sector {relative}.")


def main(verifier=verify, description=__doc__):
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument("disk", type=int, help="Windows disk number for the direct USB reader")
    parser.add_argument("reference", type=Path, help="Original MSPART.BIN for this trial")
    args = parser.parse_args()
    if args.disk < 0:
        parser.error("Enter a nonnegative disk number.")
    reference = args.reference.read_bytes()
    require(hashlib.sha256(reference).hexdigest() == REFERENCE_SHA256, "Incorrect test reference.")
    geometry = json.loads(subprocess.check_output(
        ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command",
         f"Get-Disk -Number {args.disk} -ErrorAction Stop | Select-Object Size, LogicalSectorSize,"
         " @{Name='Bus';Expression={$_.BusType.ToString()}} | ConvertTo-Json -Compress"],
        text=True,
    ))
    require((geometry["Size"], geometry["LogicalSectorSize"], geometry["Bus"]) ==
            (DISK_SECTORS * SECTOR, SECTOR, "USB"), "Unexpected disk size, sector size or bus.")
    with open(rf"\\.\PhysicalDrive{args.disk}", "rb", buffering=0) as disk, mmap.mmap(-1, SECTOR) as block:
        def read(lba):
            disk.seek(lba * SECTOR)
            require(disk.readinto(block) == SECTOR, f"Short read at LBA {lba}.")
            return block[:]

        return verifier(read, reference)


if __name__ == "__main__":
    main()
    print("PASS: exact MBR, five unchanged witnesses and empty partition 3 FAT32 metadata.")
