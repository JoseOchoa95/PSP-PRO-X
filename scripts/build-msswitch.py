"""Build and package the standalone partition switch or FAT32 format trial."""

import argparse
import hashlib
import json
from pathlib import Path
import runpy
import struct
import subprocess
import sys
import uuid
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[1]
TOOL = Path("firmware/tools/msswitch")
DIAG = Path("firmware/tools/msdiag")
SHARED = runpy.run_path(str(ROOT / "scripts/build-msdiag.py"))
LICENSES = [Path("docs/licenses") / name for name in (
    "PSPSDK-LICENSE", "COPYING.NEWLIB", "GCC-GPL-3", "GCC-RUNTIME-EXCEPTION",
)]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def verify_binary(data, symbols, build_id, title, physical, directories, directory_card, whole_card, auto_format,
                  write_path, blank_trial=False):
    if data[:4] != b"\0PBP" or len(data) < 40:
        raise ValueError("Invalid PBP header")
    version, *offsets = struct.unpack_from("<9I", data, 4)
    if version != 0x10000 or offsets[0] != 40 or offsets != sorted(offsets) or offsets[-1] != len(data):
        raise ValueError("Invalid PBP offsets")
    elf = data[offsets[6]:offsets[7]]
    if (title.encode() + b"\0" not in data[offsets[0]:offsets[1]]
            or not elf.startswith(b"\x7fELF") or build_id.encode() + b"\0" not in elf):
        raise ValueError("Invalid trial EBOOT identity")
    names = {line.split()[-1] for line in symbols.splitlines() if line.strip()}
    if write_path:
        required = {"stock_format_transaction", "stock_format_locked_write_status", "stock_format_locked_read",
                    "ms_auto_measure", "ms_auto_check_boundary", "ms_card_matches",
                    "sony_fat_stock_state", "sceIoOpen", "sceIoClose", "sceIoUnassign", "sctrlKernelExitVSH"}
        required |= ({"ms_blank_probe_plan", "ms_blank_probe_capture", "ms_blank_probe_matches", "ms_blank_probe_run"}
                     if blank_trial else {"ms_probe_sector"})
        forbidden = {"ms_format_execute", "stock_write_trial", "stock_format_partition_trial",
                     "stock_format_refresh_media", "ms_mount_first", "sony_fat_stock_reprobe",
                     "sceKernelSetEventFlag", "sceIoWrite", "sceIoDevctl"}
        if not required <= names or forbidden & names or any(
                path in elf for path in (b"MSPART.BIN", b"MSDREF.BIN", b"flash0:")):
            raise ValueError("Unexpected single-sector write path inventory")
        return
    required = {"stock_read_sector", "native_fat_check", "native_fat_mount", "native_fat_read",
                "native_fat_format_ram", "fat32_initialize", "fat32_ram_verify",
                "native_fat_unmount", "sceIoAddDrv", "sceIoDelDrv", "sceIoOpen",
                "sceIoRead", "sceIoLseek", "sceIoClose", "sctrlKernelExitVSH"}
    forbidden = {"stock_write_trial", "sceIoWrite", "sceIoAssign", "sceIoUnassign", "sceIoDevctl"}
    physical_symbols = {"stock_format_partition_trial", "native_fat_format_card"}
    if physical:
        required |= physical_symbols
    else:
        forbidden |= physical_symbols
    if directories:
        required = {"sony_mkdir_ram_check", "sony_mkdir_ram_run", "fat32_ram_finish_directories",
                    "sceIoAddDrv", "sceIoDelDrv", "sceIoOpen", "sceIoClose", "sctrlKernelExitVSH"}
        forbidden |= {"native_fat_format_ram", "sceKernelCreateSema", "sceKernelDeleteSema"}
        if directory_card:
            required |= {"dirs_card_preview", "dirs_card_write", "stock_read_sector",
                         "stock_format_partition_trial", "sceIoRead", "sceIoLseek"}
            forbidden.discard("stock_format_partition_trial")
            if b"flash0:" in elf or b"pxdirs0:" not in elf:
                raise ValueError("Unexpected physical directory trial storage paths")
        else:
            forbidden |= {"stock_read_sector", "sceIoRead", "sceIoLseek"}
            if any(path in elf for path in (b"msstor:", b"ms0:", b"fatms0:", b"flash0:")):
                raise ValueError("The RAM directory trial includes a physical storage path")
    if whole_card:
        required.discard("stock_format_partition_trial")
        required |= {"ms_format_preview", "ms_format_execute", "stock_format_transaction",
                     "stock_format_locked_write", "stock_format_refresh_media", "sony_fat_stock_state",
                     "sony_fat_stock_reprobe", "sceKernelSetEventFlag", "sceKernelReferEventFlagStatus",
                     "estimate_capacity", "sceIoGetstat",
                     "sceIoAssign", "sceIoUnassign", "sceIoDevctl"}
        forbidden -= {"sceIoAssign", "sceIoUnassign", "sceIoDevctl"}
        if any(path not in elf for path in (b"msstor0:\0", b"msstor0p1:\0", b"fatms0:\0", b"ms0:/PSP/GAME\0")):
            raise ValueError("Missing stock partition 1 mount paths")
    if auto_format:
        required |= {"ms_auto_measure", "ms_auto_check_boundary", "ms_card_capture", "ms_card_matches",
                     "ms_probe_sector"}
        if not write_path:
            required |= {"ms_gpt_inspect", "dirs_card_preview_current", "dirs_card_start_monitor",
                         "dirs_card_stop_monitor",
                         "ms_session_mark", "ms_session_observe", "ms_session_notify",
                         "sceKernelCreateCallback", "sceKernelCheckCallback", "sceKernelDeleteCallback",
                         "dirs_card_qualify", "ms_blank_capacity_capture", "ms_blank_capacity_restored",
                         "ms_blank_probe_run"}
        if any(name in elf for name in (b"MSPART.BIN", b"MSDREF.BIN")):
            raise ValueError("The automatic formatter embeds a per-card reference filename")
    if not required <= names or (forbidden & names):
        raise ValueError("Unexpected trial I/O inventory")


def compile_in(directory, image, build_id, log, run_host_tests, title, physical, directories, directory_card,
               whole_card, auto_format, write_path, blank_trial=False):
    checks = ""
    if run_host_tests:
        checks = ('gcc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined '
                  '-fno-sanitize-recover=all -I firmware/tools/msdiag -I firmware/tools/msswitch '
                  'firmware/tools/msswitch/partition_view.c tests/native/test_msswitch.c '
                  '-o test-msswitch; ./test-msswitch; '
                  'gcc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined '
                  '-fno-sanitize-recover=all -I firmware/tools/msdiag -I firmware/tools/msswitch '
                  'firmware/tools/msswitch/fat32_probe.c tests/native/test_msswitch_fat32.c '
                  '-o test-msswitch-fat32; ./test-msswitch-fat32; '
                  'gcc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined '
                  '-fno-sanitize-recover=all -I firmware/tools/msformat '
                  'firmware/tools/msformat/fat32_init.c firmware/tools/msformat/fat32_ram.c '
                  'tests/native/test_msformat_fat32.c -o test-msformat-fat32; ./test-msformat-fat32')
        if directories:
            checks += ('; gcc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -DMSDIRS_TRIAL '
                       '-fno-sanitize-recover=all -I firmware/tools/msformat '
                       'firmware/tools/msformat/fat32_init.c firmware/tools/msformat/fat32_ram.c '
                       'firmware/tools/msformat/fat32_dirs.c tests/native/test_msdirs_ram.c '
                       '-o test-msdirs-ram; ./test-msdirs-ram')
        if whole_card:
            flags = ('gcc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined '
                     '-fno-sanitize-recover=all -DMSDIRS_TRIAL -I firmware/tools/msformat ')
            checks += ('; ' + flags + 'firmware/tools/msformat/fat32_init.c firmware/tools/msformat/layout.c '
                       'tests/native/test_ms_format_layout.c -o test-ms-format-layout; ./test-ms-format-layout; '
                       + flags + 'firmware/tools/msformat/fat32_init.c firmware/tools/msformat/fat32_ram.c '
                       'firmware/tools/msformat/fat32_dirs.c firmware/tools/msformat/layout.c '
                       'firmware/tools/msformat/job.c tests/native/test_ms_format_job.c '
                       '-o test-ms-format-job; ./test-ms-format-job')
        if auto_format:
            checks += ('; ' + flags + 'firmware/tools/msformat/fat32_init.c firmware/tools/msformat/layout.c '
                       'firmware/tools/msformat/gpt.c tests/native/test_ms_format_gpt.c -lz '
                       '-o test-ms-format-gpt; ./test-ms-format-gpt')
            checks += ('; ' + flags + '-I firmware/tools/msdiag firmware/tools/msdiag/auto_probe.c '
                       'firmware/tools/msdiag/capacity.c tests/native/test_msdiag_auto_probe.c '
                       '-o test-ms-auto-probe; ./test-ms-auto-probe; '
                       + flags + 'firmware/tools/msformat/fat32_init.c firmware/tools/msformat/layout.c '
                       'firmware/tools/msformat/card_snapshot.c tests/native/test_ms_card_snapshot.c '
                       '-o test-ms-card-snapshot; ./test-ms-card-snapshot')
            checks += ('; ' + flags + 'firmware/tools/msformat/fat32_init.c firmware/tools/msformat/layout.c '
                       'firmware/tools/msformat/write_probe.c tests/native/test_ms_write_probe.c '
                       '-o test-ms-write-probe; ./test-ms-write-probe')
            if not write_path:
                checks += ('; ' + flags + '-I firmware/tools/msdiag firmware/tools/msdiag/media_session.c '
                           'tests/native/test_msdiag_media_session.c -o test-ms-media-session; ./test-ms-media-session')
                checks += ('; ' + flags + 'firmware/tools/msformat/fat32_init.c firmware/tools/msformat/layout.c '
                           'firmware/tools/msformat/card_snapshot.c firmware/tools/msformat/blank_probe.c '
                           'firmware/tools/msformat/blank_capacity.c tests/native/test_ms_blank_capacity.c '
                           '-o test-ms-blank-capacity; ./test-ms-blank-capacity')
        if blank_trial or (auto_format and not write_path):
            checks += ('; ' + flags + 'firmware/tools/msformat/blank_probe.c tests/native/test_ms_blank_probe.c '
                       '-o test-ms-blank-probe; ./test-ms-blank-probe')
    command = ["docker", "run", "--rm", "--network", "none", "--mount",
               f"type=bind,source={directory},target=/src", "-w", "/src", image, "sh", "-ec",
               'make -C firmware/tools/msswitch "BUILD_ID=$1" "PHYSICAL_FORMAT_TRIAL=$2" '
               '"RAM_DIRECTORY_TRIAL=$3" "PHYSICAL_DIRECTORY_TRIAL=$4" "WHOLE_CARD_TRIAL=$5" "AUTO_FORMAT_TRIAL=$6" '
               '"WRITE_PATH_TRIAL=$7" "BLANK_TRIAL=$8"; '
               'psp-nm firmware/tools/msswitch/msswitch.elf > symbols.txt; ' + checks,
               "msswitch-build", build_id, str(int(physical)),
               str(int(directories and not directory_card)), str(int(directory_card)), str(int(whole_card)),
               str(int(auto_format)), str(int(write_path)), str(int(blank_trial))]
    with log.open("wb") as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text(errors="replace")[-8500:])
        raise SystemExit(result.returncode)
    binary = (directory / TOOL / "EBOOT.PBP").read_bytes()
    verify_binary(binary, (directory / "symbols.txt").read_text(), build_id, title, physical,
                  directories, directory_card, whole_card, auto_format, write_path, blank_trial)
    return binary


def write_files(directory, files):
    for name, data in files.items():
        path = directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    variants = parser.add_mutually_exclusive_group()
    variants.add_argument("--physical-format-trial", action="store_true")
    variants.add_argument("--ram-directory-trial", action="store_true")
    variants.add_argument("--physical-directory-trial", action="store_true")
    variants.add_argument("--whole-card-trial", action="store_true")
    variants.add_argument("--reference-free-format-trial", action="store_true")
    variants.add_argument("--write-path-trial", action="store_true")
    variants.add_argument("--blank-media-write-trial", action="store_true")
    args = parser.parse_args()
    physical = args.physical_format_trial
    blank_trial = args.blank_media_write_trial
    write_path = args.write_path_trial or blank_trial
    auto_format = args.reference_free_format_trial or write_path
    whole_card = args.whole_card_trial or auto_format
    directory_card = args.physical_directory_trial or whole_card
    directories = args.ram_directory_trial or directory_card
    folder, title, readme = "MSSWITCH", "PRO-X Partition Switch Trial", "README.md"
    if physical:
        folder, title, readme = "MSFORMAT", "PRO-X FAT32 Partition Format Trial", "FORMAT-TRIAL.md"
    if directories:
        folder, title, readme = "MSDIRS", "PRO-X FAT32 RAM Directory Trial", "DIRECTORY-TRIAL.md"
    if directory_card:
        folder, title, readme = "MSDIRCARD", "PRO-X FAT32 Physical Directory Trial", "DIRECTORY-CARD-TRIAL.md"
    if whole_card:
        folder, title, readme = "MSMULTI", "PRO-X Memory Stick Whole Card Trial", "WHOLE-CARD-TRIAL.md"
    if auto_format:
        folder, title, readme = "MSAUTOFMT", "PRO-X Memory Stick Auto Format", "AUTO-FORMAT-TRIAL.md"
    if write_path:
        folder, title, readme = "MSWRPATH", "PRO-X Memory Stick Write Path Trial", "WRITE-PATH-TRIAL.md"
    if blank_trial:
        folder, title, readme = "MSBLANK", "PRO-X Memory Stick Uniform Sector Trial", "BLANK-MEDIA-WRITE-TRIAL.md"
    eboot = f"release/PSP/GAME/{folder}/EBOOT.PBP"
    build_id = folder.lower() + "-" + uuid.uuid4().hex[:12]
    paths = [TOOL / name for name in (
        "main.c", "Makefile", "partition_view.c", "partition_view.h", "fat32_probe.c",
        "fat32_probe.h", "native_fat_read.c", "native_fat_read.h", "README.md",
    )]
    if physical:
        paths += [TOOL / readme, TOOL / "verify-format-trial.py"]
    if directories:
        paths += [TOOL / name for name in (readme, "dirs_main.c", "sony_mkdir_ram.c",
                                          "sony_mkdir_ram.h", "sony_mkdir_profile.inc")]
        paths += [Path("firmware/tools/msformat") / name for name in ("fat32_dirs.c", "fat32_dirs.h")]
        paths += [Path("tests/native/test_msdirs_ram.c")]
    if directory_card:
        paths += [TOOL / name for name in ("dirs_card.c", "dirs_card.h", "verify-directory-trial.py",
                                          "verify-format-trial.py")]
    if whole_card:
        paths += [TOOL / name for name in ("card_mount.c", "card_mount.h")]
        if not auto_format:
            paths += [TOOL / "verify-whole-card.py"]
        paths += [DIAG / name for name in ("capacity.c", "capacity.h")]
        paths += [Path("firmware/tools/msformat") / name for name in ("layout.c", "layout.h", "job.c", "job.h", "gpt.h")]
        paths += [Path("tests/native") / name for name in ("test_ms_format_layout.c", "test_ms_format_job.c")]
    if auto_format:
        paths += [TOOL / "auto_card.c"]
        if not write_path:
            paths += [TOOL / "verify-auto-format.py"]
            paths += [DIAG / "media_session.c", DIAG / "media_session.h",
                      Path("tests/native/test_msdiag_media_session.c")]
            paths += [Path("firmware/tools/msformat") / name for name in ("blank_capacity.c", "blank_capacity.h")]
            paths += [Path("tests/native/test_ms_blank_capacity.c")]
        paths += [DIAG / name for name in ("psp_media.c", "psp_media.h", "auto_probe.c", "auto_probe.h")]
        paths += [Path("firmware/tools/msformat") / name for name in ("card_snapshot.c", "card_snapshot.h")]
        paths += [Path("tests/native") / name for name in ("test_msdiag_auto_probe.c", "test_ms_card_snapshot.c")]
        paths += [Path("firmware/tools/msformat") / name for name in ("write_probe.c", "write_probe.h")]
        paths += [Path("tests/native/test_ms_write_probe.c")]
        paths += [Path("firmware/tools/msformat/gpt.c"), Path("tests/native/test_ms_format_gpt.c")]
    if blank_trial or (auto_format and not write_path):
        paths += [Path("firmware/tools/msformat") / name for name in ("blank_probe.c", "blank_probe.h")]
        paths += [Path("tests/native/test_ms_blank_probe.c")]
    paths += [DIAG / name for name in ("probe.c", "probe.h", "stock_reader.c", "stock_reader.h")]
    paths += [Path("firmware/tools/msformat") / name for name in (
        "fat32_init.c", "fat32_init.h", "fat32_ram.c", "fat32_ram.h",
    )]
    paths += [SHARED["SYSTEMCTRL_HEADER"], SHARED["SYSTEMCTRL_IMPORTS"], *LICENSES]
    paths += [Path(name) for name in (
        "firmware/tools/mswrite/write_trial.h", "tests/native/test_msswitch.c", "tests/native/test_msswitch_fat32.c",
        "tests/native/test_msformat_fat32.c",
        "scripts/build-msswitch.py", "scripts/build-msdiag.py", "scripts/Dockerfile", "LICENSE",
    )]
    head = (subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
            if (ROOT / ".git").exists()
            else json.loads((ROOT / "source-snapshot.json").read_text())["repository_head"])
    source = {path.as_posix(): (ROOT / path).read_bytes() for path in paths}
    source["source-snapshot.json"] = (json.dumps({"repository_head": head}) + "\n").encode()
    image = subprocess.check_output(["docker", "image", "inspect", "pro-x-toolchain:phase1",
                                     "--format", "{{.Id}}"], cwd=ROOT, text=True).strip()
    work = ROOT / "artifacts/work" / build_id
    logs = ROOT / "artifacts/logs"
    logs.mkdir(parents=True, exist_ok=True)
    write_files(work, source)
    log = logs / f"{build_id}.log"
    print(f"Build log: {log.relative_to(ROOT)}", flush=True)
    binary = compile_in(work, image, build_id, log, True, title, physical, directories, directory_card,
                        whole_card, auto_format, write_path, blank_trial)
    files = {
        eboot: binary,
        "README.md": source[(TOOL / readme).as_posix()],
        "LICENSE": source["LICENSE"],
        **{f"licenses/{path.name}": source[path.as_posix()] for path in LICENSES},
    }
    if physical or (directory_card and not write_path):
        files["verify-format-trial.py"] = source[(TOOL / "verify-format-trial.py").as_posix()]
    if directory_card and not write_path:
        files["verify-directory-trial.py"] = source[(TOOL / "verify-directory-trial.py").as_posix()]
    if whole_card and not auto_format:
        files["verify-whole-card.py"] = source[(TOOL / "verify-whole-card.py").as_posix()]
    if auto_format and not write_path:
        files["verify-auto-format.py"] = source[(TOOL / "verify-auto-format.py").as_posix()]
    manifest = {
        "tool": title,
        "build_id": build_id,
        "firmware_target": "6.61",
        "firmware_changed": False,
        "hardware_validation": "NOT RUN",
        "repository_head": head,
        "toolchain_image": image,
        "source_files_sha256": {name: sha(data) for name, data in source.items()},
        "files_sha256": {name: sha(data) for name, data in files.items()},
    }
    files["build-manifest.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    dest = ROOT / "builds" / build_id
    write_files(dest, files)
    binary_zip = dest / f"pro-x-{build_id}-661.zip"
    source_zip = dest / f"pro-x-{build_id}-661-source.zip"
    SHARED["archive_and_verify"](binary_zip, files)
    SHARED["archive_and_verify"](source_zip, source)
    if auto_format and not write_path:
        subprocess.run([sys.executable, str(dest / "verify-auto-format.py"), "--help"],
                       check=True, stdout=subprocess.DEVNULL)
    if (dest / eboot).read_bytes() != binary:
        raise ValueError("Loose EBOOT differs from the archive")
    replay = work.with_name(build_id + "-source-replay")
    with ZipFile(source_zip) as archive:
        archive.extractall(replay)
    if compile_in(replay, image, build_id, logs / f"{build_id}-source-replay.log",
                  False, title, physical, directories, directory_card, whole_card, auto_format, write_path,
                  blank_trial) != binary:
        raise ValueError("Companion source does not reproduce the EBOOT")
    checksums = "".join(f"{sha((dest / name).read_bytes())}  {name}\n"
                        for name in (binary_zip.name, source_zip.name, eboot, "build-manifest.json"))
    (dest / "SHA256SUMS").write_text(checksums, encoding="ascii")
    print(f"PASS: trial checks and companion source replay. Kit: {dest}\n{checksums}", end="")


if __name__ == "__main__":
    main()
