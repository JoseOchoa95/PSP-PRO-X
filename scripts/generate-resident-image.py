"""Bind both the installer and VSH loader to the exact built user PRX."""
import hashlib
from pathlib import Path
import re
import sys
import zlib


def main():
    worker, build_id, header, compressed = sys.argv[1:]
    match = re.fullmatch(r'pro-x-([0-9a-f]{12})', build_id)
    data = Path(worker).read_bytes()
    if not match or not 52 <= len(data) <= 1024 * 1024 or data[:6] != b'\x7fELF\x01\x01':
        raise ValueError('Invalid resident worker identity or PRX')
    digest = hashlib.sha256(data).digest()
    Path(header).write_text(
        '/* Generated from the build worker. */\n'
        '#define PROX_RESIDENT_BUILD_ID "' + build_id + '"\n'
        '#define PROX_RESIDENT_IMAGE_PATH "flash0:/kd/_proxq-' + match[1] + '.prx"\n'
        '#define PROX_RESIDENT_IMAGE_SIZE ' + str(len(data)) + 'u\n'
        '#define PROX_RESIDENT_IMAGE_DIGEST "' + digest.hex() + '"\n'
        '#define PROX_RESIDENT_IMAGE_BYTES { ' + ', '.join(f'0x{byte:02x}' for byte in digest) + ' }\n',
        encoding='ascii')
    Path(compressed).write_bytes(zlib.compress(data, 9))


if __name__ == '__main__':
    main()
