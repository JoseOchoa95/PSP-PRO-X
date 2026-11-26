"""Build and package the standalone read-only partition switch trial."""

import hashlib
import json
from pathlib import Path
import runpy
import struct
import subprocess
import uuid
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[1]
TOOL = Path("firmware/tools/msswitch")
DIAG = Path("firmware/tools/msdiag")
TITLE = "PRO-X Partition Switch Trial"
EBOOT = "release/PSP/GAME/MSSWITCH/EBOOT.PBP"
SHARED = runpy.run_path(str(ROOT / "scripts/build-msdiag.py"))
LICENSES = [Path("docs/licenses") / name for name in (
    "PSPSDK-LICENSE", "COPYING.NEWLIB", "GCC-GPL-3", "GCC-RUNTIME-EXCEPTION",
)]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def verify_binary(data, symbols, build_id):
    if data[:4] != b"\0PBP" or len(data) < 40:
        raise ValueError("Invalid PBP header")
    version, *offsets = struct.unpack_from("<9I", data, 4)
    if version != 0x10000 or offsets[0] != 40 or offsets != sorted(offsets) or offsets[-1] != len(data):
        raise ValueError("Invalid PBP offsets")
    elf = data[offsets[6]:offsets[7]]
    if (TITLE.encode() + b"\0" not in data[offsets[0]:offsets[1]]
            or not elf.startswith(b"\x7fELF") or build_id.encode() + b"\0" not in elf):
        raise ValueError("Invalid trial EBOOT identity")
    names = {line.split()[-1] for line in symbols.splitlines() if line.strip()}
    required = {"stock_read_sector", "sceIoAddDrv", "sceIoDelDrv", "sceIoOpen",
                "sceIoRead", "sceIoLseek", "sceIoClose", "sctrlKernelExitVSH"}
    forbidden = {"stock_write_trial", "sceIoWrite", "sceIoAssign", "sceIoUnassign", "sceIoDevctl"}
    if not required <= names or (forbidden & names):
        raise ValueError("Unexpected trial I/O inventory")


def compile_in(directory, image, build_id, log, run_host_tests):
    checks = ""
    if run_host_tests:
        checks = ('gcc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined '
                  '-fno-sanitize-recover=all -I firmware/tools/msdiag -I firmware/tools/msswitch '
                  'firmware/tools/msswitch/partition_view.c tests/native/test_msswitch.c '
                  '-o test-msswitch; ./test-msswitch')
    command = ["docker", "run", "--rm", "--network", "none", "--mount",
               f"type=bind,source={directory},target=/src", "-w", "/src", image, "sh", "-ec",
               'make -C firmware/tools/msswitch "BUILD_ID=$1"; '
               'psp-nm firmware/tools/msswitch/msswitch.elf > symbols.txt; ' + checks,
               "msswitch-build", build_id]
    with log.open("wb") as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode:
        print(log.read_text(errors="replace")[-8500:])
        raise SystemExit(result.returncode)
    binary = (directory / TOOL / "EBOOT.PBP").read_bytes()
    verify_binary(binary, (directory / "symbols.txt").read_text(), build_id)
    return binary


def write_files(directory, files):
    for name, data in files.items():
        path = directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)


def main():
    build_id = "msswitch-" + uuid.uuid4().hex[:12]
    paths = [TOOL / name for name in ("main.c", "Makefile", "partition_view.c", "partition_view.h", "README.md")]
    paths += [DIAG / name for name in ("probe.c", "probe.h", "stock_reader.c", "stock_reader.h")]
    paths += [SHARED["SYSTEMCTRL_HEADER"], SHARED["SYSTEMCTRL_IMPORTS"], *LICENSES]
    paths += [Path(name) for name in (
        "firmware/tools/mswrite/write_trial.h", "tests/native/test_msswitch.c",
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
    binary = compile_in(work, image, build_id, log, True)
    files = {
        EBOOT: binary,
        "README.md": source[(TOOL / "README.md").as_posix()],
        "LICENSE": source["LICENSE"],
        **{f"licenses/{path.name}": source[path.as_posix()] for path in LICENSES},
    }
    manifest = {
        "tool": TITLE,
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
    if (dest / EBOOT).read_bytes() != binary:
        raise ValueError("Loose EBOOT differs from the archive")
    replay = work.with_name(build_id + "-source-replay")
    with ZipFile(source_zip) as archive:
        archive.extractall(replay)
    if compile_in(replay, image, build_id, logs / f"{build_id}-source-replay.log", False) != binary:
        raise ValueError("Companion source does not reproduce the EBOOT")
    checksums = "".join(f"{sha((dest / name).read_bytes())}  {name}\n"
                        for name in (binary_zip.name, source_zip.name, EBOOT, "build-manifest.json"))
    (dest / "SHA256SUMS").write_text(checksums, encoding="ascii")
    print(f"PASS: trial checks and companion source replay. Kit: {dest}\n{checksums}", end="")


if __name__ == "__main__":
    main()
