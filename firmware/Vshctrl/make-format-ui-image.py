"""Embed the relocation-free PSP user-mode Settings UI bridge."""

from pathlib import Path
import re
import subprocess
import sys


object_file, binary, header = map(Path, sys.argv[1:])
info = subprocess.check_output(['psp-readelf', '-rW', object_file], text=True)
sections = re.findall(r"Relocation section '([^']+)'", info)
if any(section != '.rel.pdr' for section in sections):
    raise SystemExit('Settings bridge must contain no executable relocations')
subprocess.run(['psp-objcopy', '-O', 'binary', '-j', '.text', object_file, binary], check=True)
symbols = subprocess.check_output(['psp-nm', '-n', object_file], text=True)
addresses = dict((name, int(address, 16)) for address, name in re.findall(
    r'^([\da-f]+) T prox_format_ui_(\w+)$', symbols, flags=re.MULTILINE))
entries = ('open', 'warning', 'preview_result', 'warning_result', 'run', 'scan', 'scanned')
data = binary.read_bytes()
if (not data or len(data) % 4 or len(data) > 4096 or set(addresses) != set(entries) or
        any(address % 4 or address >= len(data) for address in addresses.values())):
    raise SystemExit('Unexpected Settings bridge section or entry point')
lines = ['/* Generated from the PRO-X user-mode format UI. */',
         *(f'#define FORMAT_UI_{name.upper()} {addresses[name]}' for name in entries),
         'static const unsigned char format_ui_code[] = {']
lines.extend('    ' + ', '.join(f'0x{byte:02x}' for byte in data[i:i+16]) + ','
             for i in range(0, len(data), 16))
header.write_text('\n'.join(lines + ['};', '']), encoding='ascii')
