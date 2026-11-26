"""Read sample sectors on Windows with the microSD in a direct reader."""

import argparse
import hashlib
import json
import mmap
import os
from pathlib import Path
import struct
import subprocess
import tempfile

SECTOR_BYTES = 512
CUTOFFS = (128_000_000_000 // SECTOR_BYTES, 128 * 1024**3 // SECTOR_BYTES)
WRAP_PERIODS = (4 * 1024**3 // SECTOR_BYTES, *CUTOFFS)
WINDOW = 2048
MISSING_LBA = 2**64 - 1
HEADER = struct.Struct("<8sQII")
RECORD = struct.Struct("<Q20s")


def sample_digest(read_sector, lba):
    data = read_sector(lba)
    if len(data) != SECTOR_BYTES:
        raise OSError(f"Short read at LBA {lba}: {len(data)} bytes")
    return hashlib.sha1(data).digest(), data.count(data[:1]) == SECTOR_BYTES


def choose_sample(read_sector, target, step, sectors):
    if not 0 <= target < sectors:
        return MISSING_LBA, bytes(20)
    original, _ = sample_digest(read_sector, target)
    # Sample positions over one MiB so unused zeroed regions stay inconclusive.
    for distance in (0, 1, 2, 4, 8, *range(16, WINDOW, 64), WINDOW - 1):
        lba = target + step * distance
        if not 0 <= lba < sectors:
            continue
        digest, uniform = sample_digest(read_sector, lba)
        if not uniform and all(
            digest != sample_digest(read_sector, lba % cutoff)[0]
            for cutoff in WRAP_PERIODS if lba >= cutoff
        ):
            return lba, digest
    return target, original


def create_reference(read_sector, size, sector_size):
    if sector_size != SECTOR_BYTES:
        raise ValueError(f"Unsupported PC logical sector size: {sector_size} bytes")
    if size % SECTOR_BYTES or not WINDOW * SECTOR_BYTES < size < 2**63:
        raise ValueError(f"Invalid disk size: {size} bytes")
    sectors = size // SECTOR_BYTES
    probes = [(0, 0), (WINDOW, 0)]
    for cutoff in CUTOFFS:
        probes.extend(((cutoff - 1, -1), (cutoff, 1)))
    probes.extend(((sectors - 2, -1), (sectors - 1, 0)))
    records = (choose_sample(read_sector, lba, step, sectors) if step
               else (lba, sample_digest(read_sector, lba)[0]) for lba, step in probes)
    return (HEADER.pack(b"MSDREF01", size, sector_size, 0)
            + b"".join(RECORD.pack(*record) for record in records))


def get_disk(number):
    command = (
        "$ErrorActionPreference='Stop'; "
        f"Get-Disk -Number {number} | "
        "Select-Object Number,BusType,Size,LogicalSectorSize,IsBoot,IsSystem | "
        "ConvertTo-Json -Compress"
    )
    data = json.loads(subprocess.check_output(
        ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", command],
        text=True,
    ))
    if not data or data["IsBoot"] or data["IsSystem"]:
        raise ValueError("Select the disk number of the microSD in the direct reader")
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("disk_number", type=int, help="Physical disk number shown by Get-Disk")
    args = parser.parse_args()
    if os.name != "nt" or args.disk_number < 0:
        parser.error("Use a nonnegative Windows disk number")
    try:
        disk = get_disk(args.disk_number)
        print(f'Disk {disk["Number"]}, bus {disk["BusType"]}: {disk["Size"]} bytes, '
              f'{disk["LogicalSectorSize"]} bytes per logical sector')
        with (open(rf"\\.\PhysicalDrive{args.disk_number}", "rb", buffering=0) as device,
              mmap.mmap(-1, SECTOR_BYTES) as buffer):
            def read_sector(lba):
                device.seek(lba * SECTOR_BYTES)
                return buffer[:device.readinto(buffer)]

            reference = create_reference(read_sector, disk["Size"], disk["LogicalSectorSize"])
        output = Path(tempfile.mkdtemp(prefix="msdiag-reference-")) / "MSDREF.BIN"
        output.write_bytes(reference)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"Reference capture failed: {error}\n")
    print(f"Reference saved on the PC at {output}")


if __name__ == "__main__":
    main()
