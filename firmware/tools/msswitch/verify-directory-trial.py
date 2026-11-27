"""Verify the initial Memory Stick contents using the Windows reader's direct slot."""

from pathlib import Path
import runpy
import struct

COMMON = runpy.run_path(str(Path(__file__).with_name("verify-format-trial.py")))
START = COMMON["START"]
LENGTH = COMMON["LENGTH"]
FAT_SECTORS = COMMON["FAT_SECTORS"]
SECTOR = COMMON["SECTOR"]
require = COMMON["require"]
CLUSTER_SECTORS = 64
EXPECTED = {
    "": {"PSP", "MP_ROOT", "MUSIC", "PICTURE", "VIDEO", "ISO", "SEPLUGINS"},
    "/PSP": {"GAME", "SAVEDATA", "COMMON", "SYSTEM", "THEME", "RSSCH"},
    "/MP_ROOT": {"100MNV01", "101ANV01"},
    "/PSP/RSSCH": {"IMPORT"},
    "/ISO": {"VIDEO"},
}
INDICATORS = {b"MEMSTICKIND", b"MSTK_PROIND"}
DIRECTORY_COUNT = sum(len(children) for children in EXPECTED.values())


def short_checksum(name):
    value = 0
    for byte in name:
        value = ((value >> 1) + ((value & 1) << 7) + byte) & 0xff
    return value


def is_seplugins(names, entry):
    if len(names) != 1 or entry[:11] != b"SEPLUG~1   ":
        return False
    name = names[0]
    return (name[0] == 0x41 and name[12] == 0 and name[13] == short_checksum(entry[:11]) and
            name[26:28] == bytes(2) and name[1:11] + name[14:26] + name[28:32] ==
            "SEPLUGINS\0".encode("utf-16le") + b"\xff\xff" * 3)


def windows_entry(names, entry):
    return (entry[:11] == b"SYSTEM~1   " and entry[11] == 0x16 and
            [name[0] for name in names] == [0x42, 1] and
            all(name[13] == short_checksum(entry[:11]) for name in names) and
            b"".join(name[1:11] + name[14:26] + name[28:32] for name in reversed(names))
            .decode("utf-16le").split("\0", 1)[0] == "System Volume Information")


def entry_cluster(entry):
    return struct.unpack_from("<H", entry, 20)[0] << 16 | struct.unpack_from("<H", entry, 26)[0]


def verify(read, reference):
    COMMON["verify_identity"](read, reference)
    COMMON["verify_boot"](read, allow_mount_status=True)
    return verify_contents(read, START, LENGTH, FAT_SECTORS, CLUSTER_SECTORS)


def verify_contents(read, partition_start, length, fat_sectors, cluster_sectors, expected_label=None):
    data_start = 32 + 2 * fat_sectors
    total_clusters = (length - data_start) // cluster_sectors
    visited = set()
    windows_cluster = 0

    def check_cluster(cluster, path):
        require(2 <= cluster <= total_clusters + 1 and cluster not in visited,
                f"Invalid or reused cluster at {path}")
        fat_sector, entry_index = divmod(cluster, SECTOR // 4)
        fat = read(partition_start + 32 + fat_sector)
        other = read(partition_start + 32 + fat_sectors + fat_sector)
        entry = struct.unpack_from("<I", fat, entry_index * 4)[0]
        mirror = struct.unpack_from("<I", other, entry_index * 4)[0]
        require(entry == mirror and entry & 0x0fffffff >= 0x0ffffff8,
                f"Unexpected FAT chain at {path}")

    def visit(path, cluster, parent_cluster):
        nonlocal windows_cluster
        check_cluster(cluster, path or "/")
        visited.add(cluster)
        start = partition_start + data_start + (cluster - 2) * cluster_sectors
        pending_names, seen, dots, indicators = [], set(), set(), set()
        volume_label = None

        def check_complete():
            require(not pending_names and seen == EXPECTED.get(path, set()),
                    f"Missing or unexpected entries at {path or '/'}")
            require(dots == ({".", ".."} if path else set()), f"Missing dot entries at {path or '/'}")
            if not path:
                require(indicators == INDICATORS, "Missing initial Memory Stick indicator files.")
                require(volume_label == expected_label, "Unexpected or missing FAT32 volume label.")

        for number in range(cluster_sectors):
            data = read(start + number)
            for offset in range(0, SECTOR, 32):
                entry = data[offset:offset + 32]
                if not entry[0]:
                    check_complete()
                    return
                if entry[11] == 0x0f:
                    pending_names.append(entry)
                    continue
                if not path and entry[11] == 0x08:
                    require(not pending_names and volume_label is None and entry[:11] == expected_label and
                            entry_cluster(entry) == 0 and entry[28:32] == bytes(4),
                            "Invalid FAT32 root volume label.")
                    volume_label = entry[:11]
                    continue
                if not path and pending_names and not windows_cluster and windows_entry(pending_names, entry):
                    windows_cluster = entry_cluster(entry)
                    require(windows_cluster >= 2, "Invalid Windows directory cluster.")
                    pending_names.clear()
                    continue
                if not path and entry[:11] in INDICATORS:
                    require(not pending_names and entry[:11] not in indicators and
                            entry[11] == 0x03 and entry[12:] == bytes(20),
                            "Unexpected Memory Stick indicator attributes or data.")
                    indicators.add(entry[:11])
                    continue
                if not path and is_seplugins(pending_names, entry):
                    pending_names.clear()
                    name = "SEPLUGINS"
                else:
                    name = entry[:8].rstrip().decode("ascii")
                require(not pending_names and entry[11] == 0x10 and entry[8:11] == b"   "
                        and entry[28:32] == bytes(4), f"Unexpected entry at {path}/{name}")
                child = entry_cluster(entry)
                if name in (".", ".."):
                    require(path and name not in dots and child == (cluster if name == "." else parent_cluster),
                            f"Invalid dot entry at {path}")
                    dots.add(name)
                else:
                    require(name in EXPECTED.get(path, set()) and name not in seen,
                            f"Unexpected or duplicate directory at {path}/{name}")
                    seen.add(name)
                    visit(path + "/" + name, child, cluster if path else 0)
        check_complete()

    visit("", 2, 0)
    allocated = DIRECTORY_COUNT + 1
    require(len(visited) == allocated, "The initial tree does not contain 17 directories.")
    if windows_cluster:
        check_cluster(windows_cluster, "/System Volume Information")
    info, backup = read(partition_start + 1), read(partition_start + 7)
    for sector in (info, backup):
        require(struct.unpack_from("<I", sector)[0] == 0x41615252 and
                struct.unpack_from("<I", sector, 484)[0] == 0x61417272 and
                struct.unpack_from("<I", sector, 508)[0] == 0xaa550000,
                "Unexpected FAT32 free-cluster summary signatures.")
    require(struct.unpack_from("<II", backup, 488) == (total_clusters - allocated, allocated + 2),
            "Unexpected backup free-cluster summary.")
    if not windows_cluster:
        require(info == backup, "The primary free-cluster summary changed.")
    return bool(windows_cluster)


if __name__ == "__main__":
    windows_created = COMMON["main"](verify, __doc__)
    print("PASS: exact MBR, five unchanged witnesses, 17 folders and two empty indicator files.")
    if windows_created:
        print("Windows mount directory observed: System Volume Information.")
