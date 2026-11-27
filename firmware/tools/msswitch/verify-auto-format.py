"""Verify an MSAUTOFMT layout using a direct USB microSD reader on Windows."""

import argparse
import json
import mmap
from pathlib import Path
import runpy
import struct
import subprocess


CONTENTS = runpy.run_path(str(Path(__file__).with_name("verify-directory-trial.py")))
require = CONTENTS["require"]
SECTOR_BYTES = 512
ALIGNMENT = 2048
SECTORS_PER_GIB = (1 << 30) // SECTOR_BYTES
CAP = 10**12 // SECTOR_BYTES
TABLE_OFFSET = 446
FAT32_LBA = 0x0C
EXTENDED_LBA = 0x0F
MIN_FAT32_CLUSTERS = 65525
MIN_FAT32_SECTORS = 32 + 2 * ((MIN_FAT32_CLUSTERS + 2 + 127) // 128) + MIN_FAT32_CLUSTERS


def partition_entries(read, lba):
    sector = read(lba)
    require(len(sector) == SECTOR_BYTES and sector[510:] == b"\x55\xaa",
            f"Invalid partition table signature at LBA {lba}.")
    entries = []
    for offset in range(TABLE_OFFSET, TABLE_OFFSET + 4 * 16, 16):
        status, _, kind, _, start, length = struct.unpack_from("<B3sB3sII", sector, offset)
        entries.append((status, kind, start, length))
    return entries


def partitions(read, capacity):
    entries = partition_entries(read, 0)
    empty = (0, 0, 0, 0)
    volumes = []
    for index, (status, kind, start, length) in enumerate(entries):
        if entries[index] == empty:
            require(volumes and all(entry == empty for entry in entries[index:]),
                    "The primary MBR entries are not contiguous.")
            break
        require(status == (0x80 if index == 0 else 0), "Unexpected active partition flag.")
        if kind == FAT32_LBA:
            volumes.append((start, length, None))
            continue
        require(index == 3 and kind == EXTENDED_LBA and start and length and start + length <= capacity,
                "Invalid MBR extended partition.")
        end = start + length
        table = start
        while table < end:
            data, next_table, *unused = partition_entries(read, table)
            require(data[0] == 0 and data[1] == FAT32_LBA and data[2] == ALIGNMENT and data[3] and
                    table + data[2] + data[3] <= end and all(entry == empty for entry in unused),
                    f"Invalid logical FAT32 entry at EBR {table}.")
            volumes.append((table + data[2], data[3], table))
            if next_table == empty:
                require(table + data[2] + data[3] == end, "The extended partition has an unused tail.")
                break
            flag, link_kind, relative, span = next_table
            next_lba = start + relative
            require(flag == 0 and link_kind == EXTENDED_LBA and
                    next_lba == table + data[2] + data[3] and next_lba < end and span == end - next_lba,
                    f"Invalid next EBR pointer at LBA {table}.")
            table = next_lba
        require(len(volumes) >= 5, "Four or fewer volumes should use primary partitions.")
    return volumes


def verify_boot(read, start, length, index, require_labels=False):
    boot = read(start)
    label = f"partition {index + 1}"
    require(boot[:11] == b"\xeb\x58\x90PROX    " and boot[510:] == b"\x55\xaa",
            f"Invalid FAT32 boot record on {label}.")
    sector, cluster, reserved, copies = struct.unpack_from("<HBHB", boot, 11)
    hidden, total, fat = struct.unpack_from("<III", boot, 28)
    require(sector == SECTOR_BYTES and cluster in (1, 2, 4, 8, 16, 32, 64) and
            (reserved, copies, hidden, total) == (32, 2, start, length) and
            struct.unpack_from("<IHH", boot, 44) == (2, 1, 6) and
            struct.unpack_from("<I", boot, 67)[0] == 0x50585041 + index,
            f"Unexpected FAT32 geometry or serial on {label}.")
    clusters = (length - reserved - copies * fat) // cluster
    require(fat and MIN_FAT32_CLUSTERS <= clusters <= 0x0FFFFFEE and fat * 128 >= clusters + 2,
            f"FAT32 allocation table cannot describe {label}.")
    backup = read(start + 6)
    require(backup[:65] + boot[65:66] + backup[66:] == boot, f"The backup boot record differs on {label}.")
    expected_label = f"PART {index + 1}".encode("ascii").ljust(11)
    stored_label = boot[71:82]
    labelled = stored_label == expected_label
    require(labelled or (not require_labels and stored_label == b"NO NAME    "),
            f"Unexpected boot label on {label}.")
    windows = CONTENTS["verify_contents"](read, start, length, fat, cluster, expected_label if labelled else None)
    if labelled:
        print(f"Label {expected_label.decode('ascii').rstrip()} matches the root and both boot records.")
    return windows


def verify(read, sectors, adapter_sectors, require_labels=False):
    usable = min(sectors, CAP)
    full = adapter_sectors // SECTORS_PER_GIB * SECTORS_PER_GIB
    require(0 < full <= adapter_sectors < usable, "Unexpected adapter or direct-reader sector count.")
    volumes = partitions(read, usable)
    require(volumes, "No FAT32 volumes found.")
    position = ALIGNMENT
    for index, (start, length, ebr) in enumerate(volumes):
        if ebr is not None:
            require(ebr == position, f"Unexpected EBR start for partition {index + 1}.")
            position += ALIGNMENT
        expected_length = min(full, usable - position)
        require(start == position and length == expected_length and length >= MIN_FAT32_SECTORS,
                f"Unexpected start or length for partition {index + 1}.")
        position += length
    next_ebr = ALIGNMENT if len(volumes) >= 4 else 0
    require(0 <= usable - position < next_ebr + MIN_FAT32_SECTORS,
            "The formatter left enough space for another volume.")
    for index, (start, length, ebr) in enumerate(volumes):
        windows = verify_boot(read, start, length, index, require_labels)
        kind = "logical" if ebr is not None else "primary"
        print(f"PASS: partition {index + 1} ({kind}), LBA {start}, {length} sectors, "
              "17 directories and two indicator files.")
        if windows:
            print("Windows added System Volume Information.")
    return volumes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("disk", type=int, help="Windows disk number for the direct USB reader")
    parser.add_argument("device_sectors", type=int, help="Expected sector count from the PSP preview")
    parser.add_argument("adapter_sectors", type=int, help="Adapter sector count from the PSP preview")
    parser.add_argument("--require-labels", action="store_true", help="Require PART labels on all volumes")
    parser.add_argument("--gpt-sectors-per-copy", type=int, default=0,
                        help="Verify that both former GPT copies were zeroed, including their headers")
    args = parser.parse_args()
    if args.disk < 0 or not 0 < args.adapter_sectors < args.device_sectors:
        parser.error("Enter a valid disk number and the two sector counts from the preview.")
    count = args.gpt_sectors_per_copy
    if count and (not 33 <= count <= 1025 or 2 * count + 1 >= args.device_sectors):
        parser.error("Enter the number of sectors of one former GPT copy, including its header.")
    geometry = json.loads(subprocess.check_output(
        ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command",
         f"Get-Disk -Number {args.disk} -ErrorAction Stop | Select-Object Size, LogicalSectorSize,"
         " IsBoot, IsSystem, @{Name='Bus';Expression={$_.BusType.ToString()}},"
         " @{Name='Style';Expression={$_.PartitionStyle.ToString()}} | ConvertTo-Json -Compress"],
        text=True,
    ))
    require((geometry["Size"], geometry["LogicalSectorSize"], geometry["Bus"], geometry["Style"]) ==
            (args.device_sectors * SECTOR_BYTES, SECTOR_BYTES, "USB", "MBR") and
            not geometry["IsBoot"] and not geometry["IsSystem"], "Unexpected disk geometry, role, bus or partition style.")
    with open(rf"\\.\PhysicalDrive{args.disk}", "rb", buffering=0) as disk, mmap.mmap(-1, SECTOR_BYTES) as block:
        def read(lba):
            require(0 <= lba < args.device_sectors, f"LBA {lba} exceeds the disk.")
            disk.seek(lba * SECTOR_BYTES)
            require(disk.readinto(block) == SECTOR_BYTES, f"Short read at LBA {lba}.")
            return block[:]

        verify(read, args.device_sectors, args.adapter_sectors, args.require_labels)
        if count:
            empty = bytes(SECTOR_BYTES)
            for offset in range(count):
                for lba in (1 + offset, args.device_sectors - 1 - offset):
                    require(read(lba) == empty, f"Former GPT metadata was not cleared at LBA {lba}.")
            print(f"PASS: all {2 * count} former GPT metadata sectors are zero.")


if __name__ == "__main__":
    main()
