"""Build and verify the standalone Memory Stick diagnostic with the pinned SDK."""

import argparse
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


def build(work, image, build_id, auto_layout=False, attribute_probe=False, replay=False, identity_probe=False,
          media_event_probe=False):
    command = [
        "docker", "run", "--rm", "--network", "none", "--mount",
        f"type=bind,source={work},target=/src", "-w", "/src", image, "sh", "-ec",
        'make -C firmware/tools/msdiag "BUILD_ID=$1" "AUTO_LAYOUT=$2" "ATTRIBUTE_PROBE=$3" "IDENTITY_PROBE=$4" '
        '"MEDIA_EVENT_PROBE=$5"; '
        'gcc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all '
        '-I firmware/tools/msdiag firmware/tools/msdiag/probe.c tests/native/test_msdiag_probe.c '
        '-o test-probe; ./test-probe; '
        'gcc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all '
        '-I firmware/tools/msdiag firmware/tools/msdiag/capacity.c tests/native/test_msdiag_capacity.c '
        '-o test-capacity; ./test-capacity; '
        'if [ "$2" = 1 ]; then '
        'gcc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all '
        '-I firmware/tools/msdiag firmware/tools/msdiag/auto_probe.c firmware/tools/msdiag/capacity.c '
        'tests/native/test_msdiag_auto_probe.c -o test-auto-probe; ./test-auto-probe; '
        'gcc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all '
        '-I firmware/tools/msformat firmware/tools/msformat/layout.c firmware/tools/msformat/fat32_init.c '
        'firmware/tools/msformat/card_snapshot.c tests/native/test_ms_card_snapshot.c '
        '-o test-card-snapshot; ./test-card-snapshot; fi; '
        'if [ "$4" = 1 ]; then '
        'gcc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all '
        '-I firmware/tools/msdiag firmware/tools/msdiag/identity_probe.c firmware/tools/msdiag/probe.c '
        'tests/native/test_msdiag_identity.c -o test-identity; ./test-identity; fi; '
        'if [ "$5" = 1 ]; then '
        'gcc -std=c11 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all '
        '-I firmware/tools/msdiag firmware/tools/msdiag/media_session.c '
        'tests/native/test_msdiag_media_session.c -o test-session; ./test-session; fi; '
        'psp-nm firmware/tools/msdiag/msdiag.elf > symbols.txt',
        "msdiag-build", build_id, str(int(auto_layout)), str(int(attribute_probe)), str(int(identity_probe)),
        str(int(media_event_probe)),
    ]
    log = ROOT / "artifacts/logs" / f"{build_id}{'-source-replay' if replay else ''}.log"
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("wb") as stream:
        result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT)
    print(f"Build log: {log.relative_to(ROOT)}", flush=True)
    if result.returncode:
        print(log.read_text(errors="replace")[-9000:])
        raise SystemExit(result.returncode)
    return command


def verify_executable(binary, symbols, build_id, auto_layout=False, attribute_probe=False, identity_probe=False,
                      media_event_probe=False):
    if len(binary) < 40 or binary[:4] != b"\0PBP":
        raise ValueError("Invalid PBP")
    version, *offsets = struct.unpack_from("<9I", binary, 4)
    if version != 0x10000 or offsets[0] != 40 or offsets != sorted(offsets) or offsets[-1] != len(binary):
        raise ValueError("Invalid PBP offsets")
    param = binary[offsets[0]:offsets[1]]
    executable = binary[offsets[6]:offsets[7]]
    title = b"PRO-X Memory Stick Auto Preview\0" if auto_layout else b"PRO-X Memory Stick Diagnostic\0"
    if attribute_probe:
        title = b"PRO-X Memory Stick Attribute Probe\0"
    if identity_probe:
        title = b"PRO-X Memory Stick Identity Probe\0"
    if media_event_probe:
        title = b"PRO-X Memory Stick Change Probe\0"
    if (not param.startswith(b"\0PSF") or title not in param
            or not executable.startswith(b"\x7fELF") or build_id.encode() + b"\0" not in executable):
        raise ValueError("Unexpected PBP title, executable or build ID")
    names = {line.split()[-1] for line in symbols.splitlines() if line.strip()}
    io_symbols = {name for name in names if name.startswith("sceIo")}
    read_only = {"sceIoOpen", "sceIoRead", "sceIoLseek", "sceIoClose", "sceIoDevctl", "sceIoIoctl",
                 "sceIoChdir", "sceIoDopen", "sceIoDclose"}
    required = {"sceIoOpen", "sceIoRead", "sceIoLseek", "sceIoClose", "sceIoDevctl", "sceIoIoctl"}
    if auto_layout:
        required.discard("sceIoDevctl")
        if any(name in executable for name in (b"MSPART.BIN", b"MSDREF.BIN", b"flash0:")) or not {
                "probe_auto_capacity", "ms_format_layout", "ms_format_volume", "ms_card_capture"} <= names:
            raise ValueError("Unexpected capacity preview dependencies")
    if attribute_probe:
        required = {"sceIoOpen", "sceIoClose", "sceIoIoctl"}
        read_only = required | {"sceIoChdir", "sceIoDopen", "sceIoDclose"}
        if any(name in executable for name in (b"MSPART.BIN", b"MSDREF.BIN", b"flash0:")) or not {
                "stock_read_attribute_page", "decode_attribute_directory"} <= names:
            raise ValueError("Unexpected attribute probe dependencies")
    if identity_probe and not {"ms_identity_system_info_offset", "ms_identity_decode", "sceKernelUtilsSha1Digest"} <= names:
        raise ValueError("Missing identity probe dependencies")
    if media_event_probe:
        required = {"sceIoDevctl"}
        read_only = required | {"sceIoChdir", "sceIoDopen", "sceIoDclose"}
        callbacks = {"sceKernelCreateCallback", "sceKernelCheckCallback", "sceKernelDeleteCallback",
                     "ms_session_mark", "ms_session_notify", "ms_session_observe"}
        if not callbacks <= names or any(path in executable for path in (b"msstor:\0", b"flash0:\0")):
            raise ValueError("Unexpected medium-change probe dependencies")
    if not required <= io_symbols <= read_only:
        raise ValueError(f"Unexpected PSP I/O symbols: {sorted(io_symbols)}")
    if ("sctrlKernelExitVSH" not in names or "sceKernelExitGame" in names
            or b"SystemCtrlForKernel\0" not in executable
            or struct.pack("<I", 0x2794CCF4) not in executable):
        raise ValueError("Missing or incorrect kernel VSH return import")
    if not media_event_probe:
        stock = {"sceKernelFindModuleByName", "stock_reader_init"}
        if not attribute_probe:
            stock |= {"stock_read_sector", "estimate_capacity"}
        if not stock <= names:
            raise ValueError("Missing stock-driver transfer diagnostic")


def archive_and_verify(path, entries):
    with ZipFile(path, "w", ZIP_DEFLATED) as archive:
        for name, data in entries.items():
            archive.writestr(name, data)
    with ZipFile(path) as archive:
        if archive.namelist() != list(entries) or any(archive.read(n) != data for n, data in entries.items()):
            raise ValueError(f"Unexpected ZIP inventory or contents: {path.name}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    variants = parser.add_mutually_exclusive_group()
    variants.add_argument("--auto-layout", action="store_true", help="Build the reference-free format preview")
    variants.add_argument("--attribute-probe", action="store_true", help="Inspect Memory Stick PRO attributes")
    variants.add_argument("--identity-probe", action="store_true", help="Compare adapter-exposed identity fields")
    variants.add_argument("--media-event-probe", action="store_true", help="Observe insertion and ejection events")
    args = parser.parse_args()
    auto_layout, identity_probe, media_event_probe = args.auto_layout, args.identity_probe, args.media_event_probe
    attribute_probe = args.attribute_probe or identity_probe
    folder, title, readme = "MSDIAG", "PRO-X Memory Stick Diagnostic", "README.md"
    if auto_layout:
        folder, title, readme = "MSAUTO", "PRO-X Memory Stick Auto Preview", "AUTO-PREVIEW.md"
    if attribute_probe:
        folder, title, readme = "MSATTR", "PRO-X Memory Stick Attribute Probe", "ATTRIBUTE-PROBE.md"
    if identity_probe:
        folder, title, readme = "MSID", "PRO-X Memory Stick Identity Probe", "IDENTITY-PROBE.md"
    if media_event_probe:
        folder, title, readme = "MSCHANGE", "PRO-X Memory Stick Change Probe", "MEDIA-CHANGE-PROBE.md"
    build_id = folder.lower() + "-" + uuid.uuid4().hex[:12]
    copy_path = f"release/PSP/GAME/{folder}/EBOOT.PBP"
    work = ROOT / "artifacts/work" / build_id
    work.mkdir(parents=True)
    tool_files = tuple(TOOL / name for name in (
        "Makefile", "main.c", "probe.c", "probe.h", "capacity.c", "capacity.h", "stock_reader.c", "stock_reader.h",
        "README.md", "capture-reference.py",
    ))
    if auto_layout:
        tool_files += tuple(TOOL / name for name in ("auto_probe.c", "auto_probe.h", "auto_main.c", "psp_media.c", "psp_media.h", "AUTO-PREVIEW.md"))
        tool_files += tuple(Path("firmware/tools/msformat") / name for name in
                            ("layout.c", "layout.h", "fat32_init.c", "fat32_init.h", "card_snapshot.c",
                             "card_snapshot.h", "fat32_dirs.h", "fat32_ram.h", "gpt.h"))
        tool_files += (Path("tests/native/test_msdiag_auto_probe.c"), Path("tests/native/test_ms_card_snapshot.c"))
    if attribute_probe:
        tool_files += (TOOL / "attribute_main.c", TOOL / readme)
    if identity_probe:
        tool_files += (TOOL / "identity_probe.c", TOOL / "identity_probe.h", Path("tests/native/test_msdiag_identity.c"))
    if media_event_probe:
        tool_files += tuple(TOOL / name for name in ("media_event_main.c", "media_session.c", "media_session.h", readme))
        tool_files += (Path("tests/native/test_msdiag_media_session.c"),)
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
    command = build(work, image, build_id, auto_layout, attribute_probe, identity_probe=identity_probe,
                    media_event_probe=media_event_probe)
    binary = (work / TOOL / "EBOOT.PBP").read_bytes()
    verify_executable(binary, (work / "symbols.txt").read_text(), build_id, auto_layout, attribute_probe,
                      identity_probe, media_event_probe)
    files = {
        copy_path: binary, "README.md": source[(TOOL / readme).as_posix()],
        "LICENSE": source["LICENSE"],
        **{f"licenses/{path.name}": source[path.as_posix()] for path in licenses},
    }
    if not (auto_layout or attribute_probe or media_event_probe):
        files["capture-reference.py"] = source[(TOOL / "capture-reference.py").as_posix()]
    manifest = {
        "tool": title,
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
    if (dest / copy_path).read_bytes() != binary:
        raise ValueError("Loose EBOOT differs from the archived EBOOT")
    if auto_layout or attribute_probe or media_event_probe:
        replay = work.with_name(build_id + "-source-replay")
        with ZipFile(companion) as source_zip:
            source_zip.extractall(replay)
        build(replay, image, build_id, auto_layout, attribute_probe, replay=True, identity_probe=identity_probe,
              media_event_probe=media_event_probe)
        if (replay / TOOL / "EBOOT.PBP").read_bytes() != binary:
            raise ValueError("Companion source does not reproduce the EBOOT")
    hashes = {archive.name: sha(archive.read_bytes()), companion.name: sha(companion.read_bytes()),
              copy_path: sha(binary), "build-manifest.json": sha(files["build-manifest.json"])}
    checksum = "".join(f"{digest}  {name}\n" for name, digest in hashes.items())
    (dest / "SHA256SUMS").write_text(checksum, encoding="ascii")
    print(f"PASS: PSP executable, read-only I/O, kernel VSH return import, native tests and ZIPs. Kit: {dest}")
    print(checksum, end="")


if __name__ == "__main__":
    main()
