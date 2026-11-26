"""Build and verify the standalone Memory Stick diagnostic with the pinned SDK."""

import hashlib
import json
from pathlib import Path
import struct
import subprocess
import uuid
from zipfile import ZIP_DEFLATED, ZipFile

ROOT = Path(__file__).resolve().parents[1]
TOOL = Path("firmware/tools/msdiag")
BUILD_SCRIPT = Path("scripts/build-msdiag.py")
IMAGE = "pro-x-toolchain:phase1"
COPY_PATH = "release/PSP/GAME/MSDIAG/EBOOT.PBP"
TEST = Path("tests/native/test_msdiag_probe.c")
CAPACITY_TEST = Path("tests/native/test_msdiag_capacity.c")
SYSTEMCTRL_HEADER = Path("firmware/include/systemctrl.h")
SYSTEMCTRL_IMPORTS = Path("firmware/SystemControl/libs/libpspsystemctrl_kernel/SystemCtrlForKernel.S")


def sha(data):
    return hashlib.sha256(data).hexdigest()


def build(work, image, build_id):
    command = [
        "docker", "run", "--rm", "--network", "none", "--mount",
        f"type=bind,source={work},target=/src", "-w", "/src", image, "sh", "-ec",
        'make -C firmware/tools/msdiag "BUILD_ID=$1"; '
        'gcc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all '
        '-I firmware/tools/msdiag firmware/tools/msdiag/probe.c tests/native/test_msdiag_probe.c '
        '-o test-probe; ./test-probe; '
        'gcc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all '
        '-I firmware/tools/msdiag firmware/tools/msdiag/capacity.c tests/native/test_msdiag_capacity.c '
        '-o test-capacity; ./test-capacity; '
        'psp-nm firmware/tools/msdiag/msdiag.elf > symbols.txt',
        "msdiag-build", build_id,
    ]
    log = ROOT / "artifacts/logs" / f"{build_id}.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("wb") as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
    print(f"Build log: {log.relative_to(ROOT)}", flush=True)
    if result.returncode:
        print(log.read_text(errors="replace")[-9000:])
        raise SystemExit(result.returncode)
    return command


def verify_executable(binary, symbols, build_id):
    if len(binary) < 40 or binary[:4] != b"\0PBP":
        raise ValueError("Invalid PBP")
    version, *offsets = struct.unpack_from("<9I", binary, 4)
    if version != 0x10000 or offsets[0] != 40 or offsets != sorted(offsets) or offsets[-1] != len(binary):
        raise ValueError("Invalid PBP offsets")
    param = binary[offsets[0]:offsets[1]]
    executable = binary[offsets[6]:offsets[7]]
    if (not param.startswith(b"\0PSF") or b"PRO-X Memory Stick Diagnostic\0" not in param
            or not executable.startswith(b"\x7fELF") or build_id.encode() + b"\0" not in executable):
        raise ValueError("Unexpected PBP title, executable or build ID")
    names = {line.split()[-1] for line in symbols.splitlines() if line.strip()}
    io_symbols = {name for name in names if name.startswith("sceIo")}
    read_only = {"sceIoOpen", "sceIoRead", "sceIoLseek", "sceIoClose", "sceIoDevctl", "sceIoIoctl",
                 "sceIoChdir", "sceIoDopen", "sceIoDclose"}
    required = {"sceIoOpen", "sceIoRead", "sceIoLseek", "sceIoClose", "sceIoDevctl", "sceIoIoctl"}
    if not required <= io_symbols <= read_only:
        raise ValueError(f"Unexpected PSP I/O symbols: {sorted(io_symbols)}")
    if ("sctrlKernelExitVSH" not in names or "sceKernelExitGame" in names
            or b"SystemCtrlForKernel\0" not in executable
            or struct.pack("<I", 0x2794CCF4) not in executable):
        raise ValueError("Missing or incorrect kernel VSH return import")
    if not {"sceKernelFindModuleByName", "stock_reader_init", "stock_read_sector", "estimate_capacity"} <= names:
        raise ValueError("Missing stock-driver transfer diagnostic")


def archive_and_verify(path, entries):
    with ZipFile(path, "w", ZIP_DEFLATED) as archive:
        for name, data in entries.items():
            archive.writestr(name, data)
    with ZipFile(path) as archive:
        if archive.namelist() != list(entries) or any(archive.read(n) != data for n, data in entries.items()):
            raise ValueError(f"Unexpected ZIP inventory or contents: {path.name}")


def main():
    build_id = "msdiag-" + uuid.uuid4().hex[:12]
    work = ROOT / "artifacts/work" / build_id
    work.mkdir(parents=True)
    tool_files = tuple(TOOL / name for name in (
        "Makefile", "main.c", "probe.c", "probe.h", "capacity.c", "capacity.h", "stock_reader.c", "stock_reader.h",
        "README.md", "capture-reference.py",
    ))
    licenses = tuple(Path("docs/licenses") / name for name in (
        "PSPSDK-LICENSE", "COPYING.NEWLIB", "GCC-GPL-3", "GCC-RUNTIME-EXCEPTION",
    ))
    source = {
        path.as_posix(): (ROOT / path).read_bytes()
        for path in (*tool_files, SYSTEMCTRL_HEADER, SYSTEMCTRL_IMPORTS,
                     TEST, CAPACITY_TEST, Path("tests/test_msdiag_reference.py"),
                     BUILD_SCRIPT, Path("scripts/Dockerfile"), Path("LICENSE"), *licenses)
    }
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
    command = build(work, image, build_id)
    binary = (work / TOOL / "EBOOT.PBP").read_bytes()
    verify_executable(binary, (work / "symbols.txt").read_text(), build_id)
    files = {
        COPY_PATH: binary, "README.md": source[(TOOL / "README.md").as_posix()],
        "capture-reference.py": source[(TOOL / "capture-reference.py").as_posix()],
        "LICENSE": source["LICENSE"],
        **{f"licenses/{path.name}": source[path.as_posix()] for path in licenses},
    }
    manifest = {
        "tool": "PRO-X Memory Stick Diagnostic",
        "build_id": build_id,
        "firmware_target": "6.61",
        "firmware_changed": False,
        "hardware_validation": "NOT RUN",
        "repository_head": head,
        "source_files_sha256": {name: sha(data) for name, data in source.items()},
        "files_sha256": {name: sha(data) for name, data in files.items()},
        "toolchain_image": image,
        "command": command,
    }
    files["build-manifest.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    dest = ROOT / "builds" / build_id
    for name, data in files.items():
        target = dest / name
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    archive = dest / f"pro-x-{build_id}-661.zip"
    companion = dest / f"pro-x-{build_id}-661-source.zip"
    archive_and_verify(archive, files)
    archive_and_verify(companion, source)
    if (dest / COPY_PATH).read_bytes() != binary:
        raise ValueError("Loose EBOOT differs from the archived EBOOT")
    hashes = {archive.name: sha(archive.read_bytes()), companion.name: sha(companion.read_bytes()),
              COPY_PATH: sha(binary), "build-manifest.json": sha(files["build-manifest.json"])}
    checksum = "".join(f"{digest}  {name}\n" for name, digest in hashes.items())
    (dest / "SHA256SUMS").write_text(checksum, encoding="ascii")
    print(f"PASS: PSP executable, read-only I/O, kernel VSH return import, native tests and ZIPs. Kit: {dest}")
    print(checksum, end="")


if __name__ == "__main__":
    main()
