"""Verify the three-volume FAT32 trial in the Windows reader's direct microSD slot."""

from hashlib import sha1
from pathlib import Path
import runpy
import struct


DIRECTORIES = runpy.run_path(str(Path(__file__).with_name("verify-directory-trial.py")))
COMMON = DIRECTORIES["COMMON"]
require = COMMON["require"]
PARTITIONS = ((2048, 230686720), (230688768, 230686720), (461375488, 39319552))


def verify(read, reference):
    expected_mbr = bytearray(512)
    expected_mbr[510:] = b"\x55\xaa"
    expected_mbr[440:444] = reference[-72:-68]
    for index, (start, length) in enumerate(PARTITIONS):
        flags = b"\x80\x20\x21\x00" if not index else b"\x00\xfe\xff\xff"
        struct.pack_into("<8sII", expected_mbr, 446 + index * 16, flags + b"\x0c\xfe\xff\xff", start, length)
    require(read(0) == expected_mbr, "Unexpected partition table on the trial card.")
    for lba, index in ((268435456, 2), (500694016, 3), (COMMON["DISK_SECTORS"] - 1, 6)):
        require(sha1(read(lba)).digest() == reference[8 + index * 20:28 + index * 20],
                f"The witness at LBA {lba} changed.")
    for index, (start, length) in enumerate(PARTITIONS):
        fat_sectors = (length - 32 + 128 + 8193) // 8194
        primary = read(start)
        require(primary[:11] == b"\xeb\x58\x90PROX    " and primary[510:] == b"\x55\xaa" and
                struct.unpack_from("<HBHB", primary, 11) == (512, 64, 32, 2) and
                struct.unpack_from("<III", primary, 28) == (start, length, fat_sectors) and
                struct.unpack_from("<IHH", primary, 44) == (2, 1, 6) and
                struct.unpack_from("<I", primary, 67)[0] == 0x50585041 + index,
                f"Unexpected FAT32 boot record on partition {index + 1}.")
        backup = read(start + 6)
        require(backup[:65] + primary[65:66] + backup[66:] == primary,
                f"The backup boot record differs on partition {index + 1}.")
        windows = DIRECTORIES["verify_contents"](read, start, length, fat_sectors, 64)
        report = "Windows added System Volume Information." if windows else ""
        print(f"PASS: partition {index + 1}, 17 folders and two empty indicator files. {report}".rstrip())
    return True


if __name__ == "__main__":
    COMMON["main"](verify, __doc__)
    print("PASS: exact three-volume MBR and three unchanged witnesses.")
