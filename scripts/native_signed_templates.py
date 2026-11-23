"""Generate exact supported PRO signer envelopes from retained source inputs."""
from pathlib import Path
import hashlib
import re
import sys

def render(source):
    text=source.decode('utf-8')
    arrays={}
    for kind,size in [('psp',336),('kirk',272)]:
        rows=[]
        for name in ['small2','small','big']:
            match=re.search(r'unsigned char '+kind+r'Header_'+name+r'\[\d+\]\s*=\s*\{(.*?)\};',text,re.S)
            if not match:raise RuntimeError('Missing retained signer template: '+kind+name)
            data=bytes(int(n,16) for n in re.findall(r'0x([0-9a-fA-F]{2})',match.group(1)))
            if len(data)!=size:raise RuntimeError('Wrong signer template size')
            rows.append(data)
        arrays[kind]=rows
    result='/* Generated from retained PRO signer templates; no original firmware/resource input. */\n'
    result+='#define PROX_SIGNED_TEMPLATES_SOURCE_SHA256 "'+hashlib.sha256(source).hexdigest()+'"\n'
    for kind,size in [('psp',336),('kirk',272)]:
        result+='static const unsigned char prox_signed_'+kind+'_templates[3]['+str(size)+'] = {\n'
        for data in arrays[kind]:
            result+='  {\n'+''.join('    '+','.join('0x%02x'%v for v in data[i:i+16])+',\n' for i in range(0,len(data),16))+'  },\n'
        result+='};\n'
    return result.encode('ascii')

if __name__=='__main__':
    source,destination=map(Path,sys.argv[1:])
    destination.write_bytes(render(source.read_bytes()))
