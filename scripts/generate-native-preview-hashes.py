"""Bind a no-flash diagnostic client to its exact local module and rebooter."""
import hashlib
from pathlib import Path
import sys
probe,reboot,destination=map(Path,sys.argv[1:])
text='/* Generated from this diagnostic build; never a release update contract. */\n'
for label,path in [('MODULE',probe),('REBOOT',reboot)]:
    data=path.read_bytes()
    text+=f'#define PROX_PREVIEW_{label}_SIZE {len(data)}u\n'
    text+=f'#define PROX_PREVIEW_{label}_SHA256 "{hashlib.sha256(data).hexdigest()}"\n'
destination.write_text(text,encoding='ascii')
