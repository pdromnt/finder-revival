"""Patch ONLY logo palette/pixels in a locally supplied verified stock kernel.

Produces a local untested image; never flashes or packages vendor kernel bytes.
"""
from pathlib import Path
import argparse,gzip,hashlib,json,struct,zlib
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
STOCK='ffcf3a7a15d06d42d14b104b2e63ed824888e89bbf21c4a5c330b911c5d35e87'
def repack(original,payload):
    if hashlib.sha256(original).hexdigest()!=STOCK:raise ValueError('Unsupported stock kernel hash')
    offset=0x462c;decoder=zlib.decompressobj(31);old=decoder.decompress(original[offset:])+decoder.flush()
    if not decoder.eof or len(old)!=6673472:raise ValueError('Unexpected stock gzip/kernel layout')
    length=len(original)-offset-len(decoder.unused_data)
    allowed=[(6249820,660),(6250480,153600)]
    cursor=0
    for start,size in allowed:
        if old[cursor:start]!=payload[cursor:start]:raise ValueError('Non-logo kernel modification')
        cursor=start+size
    if old[cursor:]!=payload[cursor:] or len(old)!=len(payload):raise ValueError('Non-logo kernel modification')
    compressed=gzip.compress(payload,compresslevel=9,mtime=0)
    padding=length-len(compressed)
    if padding<0:raise ValueError('Logo exceeds the original compressed slot')
    if padding:
        head=bytearray(compressed[:10]);head[3]=8
        compressed=bytes(head)+b'R'*(padding-1)+b'\0'+compressed[10:]
    result=bytearray(original);result[offset:offset+length]=compressed
    if struct.unpack_from('<I',result,offset+length-4)[0]!=len(payload):raise ValueError('Fixed-position inflated-size word lost')
    if struct.unpack_from('<I',result,offset+length-8)[0]!=(zlib.crc32(payload)&0xffffffff):raise ValueError('Fixed-position gzip CRC lost')
    struct.pack_into('>I',result,24,zlib.crc32(result[64:])&0xffffffff)
    struct.pack_into('>I',result,4,0);struct.pack_into('>I',result,4,zlib.crc32(result[:64])&0xffffffff)
    check=zlib.decompressobj(31)
    assert check.decompress(result[offset:])+check.flush()==payload and check.eof
    assert check.unused_data==original[offset+length:]
    assert len(result)==len(original) and result[64:offset]==original[64:offset]
    start=offset+10
    if result[offset+3]&8:start=result.index(0,start)+1
    raw=zlib.decompressobj(-15);assert raw.decompress(result[start:])+raw.flush()==payload and raw.eof
    assert len(result)-len(raw.unused_data)==offset+length-8
    return bytes(result)
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--input',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--image',type=Path,default=ROOT/'assets/bootscreen/revival-v3-flat.png');args=p.parse_args()
    original=args.input.read_bytes()
    if hashlib.sha256(original).hexdigest()!=STOCK:p.error('Unsupported stock kernel; do not bypass the hash guard')
    payload=bytearray(zlib.decompress(original[0x462c:],31))
    image=Image.open(args.image).convert('RGB')
    if image.size!=(480,320):p.error('Artwork must be exactly 480x320')
    indexed=image.transpose(Image.Transpose.ROTATE_270).quantize(colors=220,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
    payload[6249820:6249820+660]=bytes(indexed.getpalette()[:660])
    payload[6250480:6250480+153600]=bytes(value+32 for value in indexed.tobytes())
    result=repack(original,bytes(payload));args.output.parent.mkdir(parents=True,exist_ok=True)
    if args.output.exists():p.error('Output exists; preserve previous artifacts')
    args.output.write_bytes(result)
    report={'original_sha256':STOCK,'patched_sha256':hashlib.sha256(result).hexdigest(),'bytes':len(result),'logo_only':True,
            'padding':'internal gzip FNAME; fixed trailer preserved','boot_tested':False,'flashed':False}
    args.output.with_name(args.output.name+'.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
