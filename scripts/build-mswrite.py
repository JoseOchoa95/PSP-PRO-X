"""Build and package the standalone Memory Stick write trials."""

import argparse
import hashlib
import json
from pathlib import Path
import runpy
import struct
import subprocess
import uuid


ROOT = Path(__file__).resolve().parents[1]
TOOL = Path("firmware/tools/mswrite")
DIAG = Path("firmware/tools/msdiag")
IMAGE = "pro-x-toolchain:phase1"
SHARED = runpy.run_path(str(ROOT / "scripts/build-msdiag.py"))
PARTITION_TEST = Path("tests/native/test_mspart.c")


def verify_executable(binary, symbols, build_id, tool_title):
    if binary[:4] != b"\0PBP" or len(binary) < 40:
        raise ValueError("Invalid PBP")
    version, *offsets = struct.unpack_from("<9I", binary, 4)
    if version != 0x10000 or offsets[0] != 40 or offsets != sorted(offsets) or offsets[-1] != len(binary):
        raise ValueError("Invalid PBP offsets")
    title = binary[offsets[0]:offsets[1]]
    executable = binary[offsets[6]:offsets[7]]
    if (tool_title.encode() + b"\0" not in title
            or not executable.startswith(b"\x7fELF")
            or build_id.encode() + b"\0" not in executable):
        raise ValueError("Unexpected trial executable")
    names = {line.split()[-1] for line in symbols.splitlines() if line.strip()}
    required = {"stock_write_trial", "stock_read_sector", "sctrlKernelExitVSH", "sceIoIoctl"}
    if not required <= names or {"sceIoWrite", "sceIoAssign", "sceIoUnassign"} & names:
        raise ValueError("Unexpected trial I/O inventory")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--partition-trial", action="store_true")
    partitions = parser.parse_args().partition_trial
    tool_name = "mspart" if partitions else "mswrite"
    tool_title = "PRO-X Memory Stick Partition Trial" if partitions else "PRO-X Memory Stick Write Trial"
    copy_path = f"release/PSP/GAME/{tool_name.upper()}/EBOOT.PBP"
    readme = TOOL / ("PARTITION-TRIAL.md" if partitions else "README.md")
    build_id = tool_name + "-" + uuid.uuid4().hex[:12]
    work = ROOT / "artifacts/work" / build_id
    work.mkdir(parents=True)
    paths = [TOOL / name for name in ("main.c", "Makefile", "write_trial.h", "mswrite-reference.py")]
    paths.append(readme)
    if partitions:
        paths += [TOOL / "partition_table.c", TOOL / "partition_table.h", PARTITION_TEST]
    paths += [DIAG / name for name in ("probe.c", "probe.h", "stock_reader.c", "stock_reader.h", "capture-reference.py")]
    paths += [SHARED["SYSTEMCTRL_HEADER"], SHARED["SYSTEMCTRL_IMPORTS"]]
    licenses = [Path("docs/licenses") / name for name in (
        "PSPSDK-LICENSE", "COPYING.NEWLIB", "GCC-GPL-3", "GCC-RUNTIME-EXCEPTION",
    )]
    paths += [Path("scripts/build-mswrite.py"), Path("scripts/build-msdiag.py"),
              Path("scripts/Dockerfile"), Path("LICENSE"), *licenses]
    source = {path.as_posix(): (ROOT / path).read_bytes() for path in paths}
    for name, data in source.items():
        path = work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    if (ROOT / ".git").exists():
        head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    else:
        head = json.loads((ROOT / "source-snapshot.json").read_text())["repository_head"]
    source["source-snapshot.json"] = (json.dumps({"repository_head": head}) + "\n").encode()
    image = subprocess.check_output(["docker", "image", "inspect", IMAGE, "--format", "{{.Id}}"],
                                    cwd=ROOT, text=True).strip()
    command = ["docker", "run", "--rm", "--network", "none", "--mount",
               f"type=bind,source={work},target=/src", "-w", "/src", image, "sh", "-ec",
               'make -C firmware/tools/mswrite "BUILD_ID=$1" "PARTITION_TRIAL=$2"; '
               'psp-nm "firmware/tools/mswrite/$3.elf" > symbols.txt; '
               'if [ "$2" = 1 ]; then '
               'gcc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all '
               '-I firmware/tools/msdiag -I firmware/tools/mswrite firmware/tools/msdiag/probe.c '
               'firmware/tools/mswrite/partition_table.c tests/native/test_mspart.c -o test-mspart; '
               './test-mspart test-mbr.bin; fi',
               "mswrite-build", build_id, "1" if partitions else "0", tool_name]
    log = ROOT / "artifacts/logs" / f"{build_id}.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("wb") as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
    print(f"Build log: {log.relative_to(ROOT)}", flush=True)
    if result.returncode:
        print(log.read_text(errors="replace")[-9000:])
        raise SystemExit(result.returncode)
    binary = (work / TOOL / "EBOOT.PBP").read_bytes()
    verify_executable(binary, (work / "symbols.txt").read_text(), build_id, tool_title)

    def sha(data):
        return hashlib.sha256(data).hexdigest()

    files = {
        copy_path: binary,
        "mswrite-reference.py": source[(TOOL / "mswrite-reference.py").as_posix()],
        "capture-reference.py": source[(DIAG / "capture-reference.py").as_posix()],
        "README.md": source[readme.as_posix()],
        "LICENSE": source["LICENSE"],
        **{f"licenses/{path.name}": source[path.as_posix()] for path in licenses},
    }
    manifest = {
        "tool": tool_title,
        "build_id": build_id, "firmware_target": "6.61", "firmware_changed": False,
        "hardware_validation": "NOT RUN", "trial_lba": 0 if partitions else 500694016,
        "repository_head": head, "toolchain_image": image, "command": command,
        "source_files_sha256": {name: sha(data) for name, data in source.items()},
        "files_sha256": {name: sha(data) for name, data in files.items()},
    }
    files["build-manifest.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    dest = ROOT / "builds" / build_id
    for name, data in files.items():
        path = dest / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
    binary_zip = dest / f"pro-x-{build_id}-661.zip"
    source_zip = dest / f"pro-x-{build_id}-661-source.zip"
    SHARED["archive_and_verify"](binary_zip, files)
    SHARED["archive_and_verify"](source_zip, source)
    if (dest / copy_path).read_bytes() != binary:
        raise ValueError("The loose trial EBOOT differs from the archive")
    checksums = "".join(f"{sha((dest / name).read_bytes())}  {name}\n"
                        for name in (binary_zip.name, source_zip.name, copy_path, "build-manifest.json"))
    (dest / "SHA256SUMS").write_text(checksums, encoding="ascii")
    print(f"PASS: trial EBOOT and package checks. Kit: {dest}\n{checksums}", end="")


if __name__ == "__main__":
    main()
