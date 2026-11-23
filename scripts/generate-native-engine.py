"""Embed the matching local writer in its client; no target/release data."""
from pathlib import Path
import hashlib,sys
source,destination=map(Path,sys.argv[1:]);data=source.read_bytes()
assert data.startswith(b'\x7fELF') and len(data)<=1024*1024
text='/* Generated from this exact local background writer build. */\n'
text+='#define PROX_NATIVE_ENGINE_SIZE '+str(len(data))+'u\n'
text+='#define PROX_NATIVE_ENGINE_SHA256 "'+hashlib.sha256(data).hexdigest()+'"\n'
text+='static const unsigned char prox_native_engine[] = {\n'
text+=''.join('    '+','.join('0x%02x'%v for v in data[i:i+16])+',\n' for i in range(0,len(data),16))
text+='};\n';destination.write_text(text,encoding='ascii')
